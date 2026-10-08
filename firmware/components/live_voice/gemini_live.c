/*
 * Gemini Live(3단계) 음성 세션: BidiGenerateContent WebSocket으로 듣기·생각·도구 호출·말하기를 한 연결에서
 * 한다(docs/design-voice-saver-mode.md 2.5·16.4절, 3단계. 2026-10 ai.google.dev/api/live,
 * /gemini-api/docs/live-api/get-started-websocket, /gemini-api/docs/live-api/tools,
 * /gemini-api/docs/live-api/capabilities 확인). live_voice.c(gpt-live-1)와 같은 구조를 따른다: 세션 태스크
 * 1개(연결·마이크 업로드) + 응답 해석 태스크 1개(esp_websocket_client, ws_stack_reserve 내부 RAM 요령도
 * 그대로). 다른 점:
 *   - 메시지 모양은 gemini_live_proto.c(순수 로직, 셀프테스트)만 안다.
 *   - 백엔드 위임이 없다 — 모델이 도구를 직접 부르면(toolCall) 응답 해석 태스크가 바로 실행하고
 *     (assistant_execute, tool_guard 그대로) toolResponse로 돌려준다. gemini-3.8-live는 비동기(NON_BLOCKING)
 *     도구 호출을 지원한다는 근거를 문서에서 찾지 못해 막힌 채로 두고, 대신 지시문에 "오래 걸리는 도구는
 *     먼저 짧게 말하라"고 적었다(요청 4). 응답 해석 태스크 안에서 실행하므로 그동안 세션 루프(마이크 업로드)는
 *     막히지 않는다.
 *   - 끼어들기는 서버 VAD가 serverContent.interrupted로 알려준다 — 재생 큐만 비우면 된다(요청 3).
 *   - 가운데 버튼(Key2) 짧게는 live_voice_barge_in()의 기본 분기(실시간과 같음, saver만 따로 처리)가 이미
 *     live_voice_stop()을 부르므로 이 파일은 따로 처리하지 않는다.
 *   - 인증은 WebSocket 업그레이드 헤더 x-goog-api-key로 보낸다(URL의 ?key=는 로그·프록시에 남을 수 있어
 *     피한다 — gemini.saver의 x-goog-api-key와 같은 이유, 2026-10 forum/문서 확인: 헤더도 받아 준다).
 *
 * 알려진 한계(문서화, 요청 4): toolCallCancellation은 응답 해석 태스크가 메시지를 하나씩 순서대로 처리하고
 * 그 안에서 도구를 동기 실행하므로, 같은 toolCall 메시지를 처리하는 도중에는 그 뒤에 온 취소를 볼 수 없다
 * (처리 전에 이미 큐에 들어와 있던 취소만 거른다). gemini-3.8-live가 동기 호출만 지원한다는 점과 맞물려
 * 실제로 취소가 늦게 와서 못 거르는 경우는 드물 것으로 본다.
 *
 * 로그는 LIVE와 같은 모양만 남긴다(GLIVE: limits …, 10초 진단, 연결·종료 사유, 도구 횟수). 받아쓴 글자·
 * AI 대답·키는 절대 로그에 남기지 않는다.
 */
#include "gemini_live.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app_config.h"
#include "audio_core.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_transport_ssl.h"
#include "esp_transport_ws.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "gemini_base64.h"
#include "gemini_live_proto.h"
#include "gemini_schema.h"
#include "hangup_phrase.h"
#include "lwip/sockets.h"
#include "net.h"
#include "saver_resample.h"
#include "saver_voice.h"
#include "secret_store.h"
#include "wake.h"

static const char *TAG = "GLIVE";

#define GEMINI_LIVE_URI \
    "wss://generativelanguage.googleapis.com/ws/google.ai.generativelanguage.v1beta.GenerativeService.BidiGenerateContent"
#define GEMINI_LIVE_PATH "/ws/google.ai.generativelanguage.v1beta.GenerativeService.BidiGenerateContent"

#define MESSAGE_MAX          (128 * 1024)   /* live_voice.c와 같은 여유(음성·도구 조각이 봉투에 같이 옴) */
#define INPUT_QUEUE_FRAMES   64
#define INPUT_FRAME_SAMPLES  512
#define INPUT_BATCH_SAMPLES  1024            /* 64ms @16kHz, LIVE 업로드 묶음과 같은 크기 */
#define OUTBOUND_QUEUE       6
#define STARTUP_TIMEOUT_MS   20000
#define CONNECT_TIMEOUT_MS   20000
#define WIFI_WAIT_MS         15000
#define CLOCK_WAIT_MS        20000
#define CANCELLED_ID_MAX     8
#define CANCELLED_ID_LEN     40
/* live_voice.c와 같다: esp_websocket_client가 만드는 내부 태스크의 스택(내부 RAM). 한 번에 하나의
 * WebSocket만 열리므로(openai.live·gemini.live가 동시에 돌지 않음, s_busy로 보장) 같은 예약 자리
 * (voice_shared_reserve_release/take, live_voice.c 소유)를 그대로 나눠 쓴다 */
#define WS_TASK_STACK        12288

typedef struct {
    size_t  bytes;
    int16_t samples[INPUT_FRAME_SAMPLES];
} input_frame_t;

static QueueHandle_t     s_inputs;          /* 마이크 → 업로드 */
static QueueHandle_t     s_responses;       /* WebSocket 수신 → 해석 태스크 (char*) */
static QueueHandle_t     s_outbound;        /* 해석 태스크(도구 응답) → 세션 태스크 (char*, 완성된 JSON) */
static SemaphoreHandle_t s_responses_idle;
static SemaphoreHandle_t s_pending_lock;
static char             *s_pending_text;     /* gemini_live_text()가 넣는 글 (PSRAM, 세션이 소비) */

static atomic_bool s_ws_connected, s_failed, s_session_active;
static char       *s_message;
static size_t      s_message_used;
static int         s_message_opcode;

/* 고정 버퍼는 모두 PSRAM (2026-10-07 실기기: 내부 RAM에 두었더니 부팅 뒤 여유가 15KB → 9.9KB로 줄어
 * 텔레그램 TLS가 "esp-aes: Failed to allocate memory"). gemini_live_init에서 잡는다 */
static char (*s_cancelled_ids)[CANCELLED_ID_LEN];
static int16_t *s_batch;            /* INPUT_BATCH_SAMPLES */
#define KEY_BUF_LEN     513
#define HEADERS_BUF_LEN 560
static char *s_key, *s_headers;
static int   s_cancelled_next;
static saver_resample_t s_resampler;
static hangup_phrase_t  s_hangup;
static atomic_uint s_tools_total;    /* 세션 전체 도구 실행 횟수 (종료 로그용) */

typedef enum { ACT_START, ACT_AUDIO, ACT_IN_TEXT, ACT_OUT_TEXT, ACT_TOOL, ACT_COUNT } activity_t;
static const char *const ACT_NAMES[ACT_COUNT] = {"start", "speaker audio", "heard speech", "assistant text",
                                                 "tool call"};
static atomic_uint s_activity_ms;
static atomic_int  s_last_act;
static atomic_uint s_act_count[ACT_COUNT];

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void touch_activity(activity_t why)
{
    atomic_store(&s_activity_ms, now_ms());
    atomic_store(&s_last_act, why);
    atomic_fetch_add(&s_act_count[why], 1);
}

/* 고정 문구만 기록한다 (live_voice.c의 fail()과 같다) */
static void fail(const char *text, app_error_t code)
{
    bool expected = false;
    if (atomic_compare_exchange_strong(&s_failed, &expected, true)) {
        voice_shared_set_status(text);
        ESP_LOGW(TAG, "%s (internal free %u)", text,
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        app_events_post(APP_EVT_ERROR, &code, sizeof(code), 0);
    }
    live_voice_stop();
}

static bool is_cancelled(const char *id)
{
    for (int i = 0; i < CANCELLED_ID_MAX; i++) {
        if (s_cancelled_ids[i][0] && !strcmp(s_cancelled_ids[i], id)) {
            return true;
        }
    }
    return false;
}

static void remember_cancelled(const char *id)
{
    snprintf(s_cancelled_ids[s_cancelled_next], CANCELLED_ID_LEN, "%s", id);
    s_cancelled_next = (s_cancelled_next + 1) % CANCELLED_ID_MAX;
}

/* ── 응답 해석 ────────────────────────────────────────────── */

/* modelTurn의 inlineData(24kHz PCM, base64)를 디코딩해 16kHz로 바꿔 재생 큐에 넣는다 */
static void handle_audio_inline(const char *b64, size_t b64_len)
{
    uint8_t *pcm = heap_caps_malloc(b64_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);   /* 디코딩 결과 <= 길이 */
    size_t pcm_len = pcm ? gemini_base64_decode(b64, b64_len, pcm, b64_len) : SIZE_MAX;
    if (!pcm || pcm_len == SIZE_MAX || (pcm_len % 2)) {
        fail("Invalid voice audio", APP_ERR_VOICE_INTERNAL);
        free(pcm);
        return;
    }
    const int16_t *pcm24 = (const int16_t *)pcm;
    size_t samples24 = pcm_len / 2;
    /* 무음 조각은 활동으로 안 센다 (서버가 말이 끝난 뒤에도 무음을 계속 보낼 수 있다. 책 16장 버그 2, 18장 18.3절) */
    for (size_t i = 0; i < samples24; i++) {
        if (pcm24[i] > 200 || pcm24[i] < -200) {
            touch_activity(ACT_AUDIO);
            break;
        }
    }
    for (size_t i = 0; i < samples24 && !voice_shared_stopping(); i += 1024) {
        size_t n = samples24 - i < 1024 ? samples24 - i : 1024;
        int16_t pcm16[700];
        size_t out_n = saver_resample_24to16(&s_resampler, pcm24 + i, n, pcm16, sizeof(pcm16) / sizeof(pcm16[0]));
        if (out_n && !audio_core_voice_enqueue(pcm16, out_n)) {
            fail("Speaker backlog", APP_ERR_VOICE_INTERNAL);
            break;
        }
    }
    free(pcm);
}

/* toolCall.functionCalls[]를 순서대로 실행하고(assistant_execute, tool_guard 그대로) 한 번에 돌려준다.
 * 모델이 응답을 기다리는 동안(동기 호출)이라 이 함수가 도는 동안 세션 루프의 마이크 업로드는 막히지 않는다
 * (이 함수는 응답 해석 태스크에서만 돈다, 세션 태스크와 다른 태스크) */
static void handle_tool_calls(const cJSON *calls)
{
    touch_activity(ACT_TOOL);
    const live_voice_handlers_t *h = voice_shared_handlers();
    cJSON *responses = cJSON_CreateArray();
    int n = 0, ignored = 0;
    const cJSON *call;
    cJSON_ArrayForEach(call, calls) {
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(call, "id");
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(call, "name");
        const cJSON *args = cJSON_GetObjectItemCaseSensitive(call, "args");
        if (!cJSON_IsString(id) || !cJSON_IsString(name)) {
            continue;
        }
        if (is_cancelled(id->valuestring)) {
            ignored++;
            continue;      /* 요청 4: 취소된 id는 결과를 무시한다 (머리말의 한계 설명 참고) */
        }
        char *args_text = args ? cJSON_PrintUnformatted(args) : NULL;
        cJSON *result = h && h->execute_tool ? h->execute_tool(name->valuestring, args_text ? args_text : "{}") : NULL;
        free(args_text);
        atomic_fetch_add(&s_tools_total, 1);
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "id", id->valuestring);
        cJSON_AddStringToObject(item, "name", name->valuestring);
        cJSON_AddItemToObject(item, "response", result ? result : cJSON_Parse("{\"ok\":false}"));
        cJSON_AddItemToArray(responses, item);
        n++;
    }
    if (ignored) {
        ESP_LOGI(TAG, "tool call ignored (cancelled): %d", ignored);
    }
    if (!n) {
        cJSON_Delete(responses);
        return;
    }
    char *text = gemini_live_build_tool_response(responses);     /* responses 소유권을 가져간다 */
    if (!text || xQueueSend(s_outbound, &text, pdMS_TO_TICKS(250)) != pdTRUE) {
        free(text);
        if (!voice_shared_stopping()) {
            fail("Device tool response failed", APP_ERR_VOICE_DROPPED);
        }
    }
}

static void parse_message(const char *text)
{
    cJSON *root = cJSON_Parse(text);
    if (!root) {
        fail("Invalid voice response", APP_ERR_VOICE_INTERNAL);
        return;
    }
    if (gemini_live_is_setup_complete(root)) {
        hangup_phrase_reset(&s_hangup);
        touch_activity(ACT_START);
        voice_shared_set_ready(true);
        voice_shared_set_status("Listening");
        voice_shared_set_state(VOICE_LIVE);
        audio_core_tone();
        ESP_LOGI(TAG, "setup complete");
        goto done;
    }
    if (gemini_live_is_goaway(root)) {
        ESP_LOGI(TAG, "server goAway (closing soon)");
        voice_shared_set_status("Gemini server closing soon");
        live_voice_stop();
        goto done;
    }
    {
        const cJSON *content = gemini_live_server_content(root);
        if (content && !voice_shared_stopping()) {
            bool interrupted = gemini_live_is_interrupted(content);
            if (interrupted) {
                audio_core_voice_clear();      /* 서버 VAD 끼어들기 (요청 3): 재생 중이던 소리만 비운다 */
            }
            const live_voice_handlers_t *h = voice_shared_handlers();
            const char *in_text = gemini_live_transcript_text(content, true);
            if (in_text) {
                touch_activity(ACT_IN_TEXT);
                if (h && h->on_transcript) {
                    h->on_transcript(true, in_text);
                }
                if (live_voice_ready()) {
                    hangup_phrase_feed(&s_hangup, in_text, now_ms());
                    if (h && h->on_user_input) {
                        h->on_user_input();     /* TLG-14: inputAudioTranscription이 "사람이 새로 말함" 신호 */
                    }
                }
            }
            const char *out_text = gemini_live_transcript_text(content, false);
            if (out_text) {
                touch_activity(ACT_OUT_TEXT);
                if (h && h->on_transcript) {
                    h->on_transcript(false, out_text);
                }
            }
            size_t b64_len = 0;
            const char *b64 = gemini_live_find_inline_audio(content, &b64_len);
            if (b64) {
                handle_audio_inline(b64, b64_len);
            }
            if (interrupted || gemini_live_is_turn_complete(content)) {
                saver_resample_reset(&s_resampler);   /* 턴 경계: 다음 오디오는 새로 이어 붙인다 */
            }
        }
        const cJSON *calls = gemini_live_tool_calls(root);
        if (calls && !voice_shared_stopping()) {
            handle_tool_calls(calls);
        }
        const cJSON *cancelled = gemini_live_cancelled_ids(root);
        if (cancelled) {
            const cJSON *id;
            cJSON_ArrayForEach(id, cancelled) {
                if (cJSON_IsString(id)) {
                    remember_cancelled(id->valuestring);
                }
            }
        }
    }
done:
    cJSON_Delete(root);
}

static void response_task(void *arg)
{
    for (;;) {
        char *text = NULL;
        if (xQueueReceive(s_responses, &text, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (text) {
                parse_message(text);
                free(text);
            } else {
                xSemaphoreGive(s_responses_idle);    /* 세션 종료 장벽 */
            }
        }
        if (live_voice_ready() && hangup_phrase_ready(&s_hangup, now_ms())) {
            ESP_LOGI(TAG, "hangup phrase heard");
            voice_shared_set_status("Hangup phrase heard");
            live_voice_stop();
            audio_core_voice_clear();
        }
    }
}

/* ── WebSocket 이벤트 (live_voice.c의 websocket_event()와 같은 모양) ─────────────── */

static void queue_message(void)
{
    s_message[s_message_used] = '\0';
    char *copy = heap_caps_malloc(s_message_used + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!copy) {
        fail("Voice response allocation failed", APP_ERR_VOICE_INTERNAL);
        return;
    }
    memcpy(copy, s_message, s_message_used + 1);
    if (xQueueSend(s_responses, &copy, 0) != pdTRUE && xQueueSend(s_responses, &copy, pdMS_TO_TICKS(250)) != pdTRUE) {
        free(copy);
        if (!voice_shared_stopping()) {
            fail("Voice response backlog", APP_ERR_VOICE_INTERNAL);
        }
    }
}

static void websocket_event(void *arg, esp_event_base_t base, int32_t id, void *raw)
{
    esp_websocket_event_data_t *event = raw;
    if (id == WEBSOCKET_EVENT_CONNECTED) {
        int fd = esp_transport_get_socket((esp_transport_handle_t)arg), enabled = 1;
        if (fd < 0 || setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled)) != 0) {
            fail("Could not configure voice socket", APP_ERR_VOICE_CONNECT);
            return;
        }
        atomic_store(&s_ws_connected, true);
        return;
    }
    if (id == WEBSOCKET_EVENT_ERROR || id == WEBSOCKET_EVENT_DISCONNECTED || id == WEBSOCKET_EVENT_CLOSED) {
        if (!voice_shared_stopping()) {
            if (event->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGW(TAG, "transport error: errno %d, tls 0x%x", event->error_handle.esp_transport_sock_errno,
                         event->error_handle.esp_tls_stack_err);
            }
            fail(event->error_handle.error_type == WEBSOCKET_ERROR_TYPE_PONG_TIMEOUT ? "Voice heartbeat timed out"
                                                                                     : "Voice connection ended",
                 APP_ERR_VOICE_DROPPED);
        }
        atomic_store(&s_ws_connected, false);
        return;
    }
    /* 서버가 연결을 닫으면 닫는 코드·이유를 남긴다 (설정 거절 등 원인 확인용. 이유는 서버의 오류 문구로, 대화 내용이 아님) */
    if (id == WEBSOCKET_EVENT_DATA && event->op_code == 8) {
        int code = event->data_len >= 2 ? (((uint8_t)event->data_ptr[0]) << 8) | (uint8_t)event->data_ptr[1] : 0;
        int reason_len = event->data_len > 2 ? event->data_len - 2 : 0;
        ESP_LOGW(TAG, "server closed: code %d, reason: %.*s", code, reason_len > 160 ? 160 : reason_len,
                 reason_len ? event->data_ptr + 2 : "");
        return;
    }
    /*
     * Gemini Live는 JSON 메시지를 이진(binary, op 2) 프레임으로 보낸다. 글자(op 1)만 받았더니 setupComplete를 버려
     * "Voice startup timed out"으로 끝났다 (2026-10-07 실기기). 글자·이진 모두 JSON으로 받는다. op 0은 이어지는 조각
     */
    if (id != WEBSOCKET_EVENT_DATA || (event->op_code != 1 && event->op_code != 2 && event->op_code != 0)) {
        return;
    }
    if (event->payload_offset == 0 && (event->op_code == 1 || event->op_code == 2)) {
        s_message_used = 0;
        s_message_opcode = event->op_code;
    }
    if ((s_message_opcode != 1 && s_message_opcode != 2) || event->data_len < 0 ||
        s_message_used + event->data_len >= MESSAGE_MAX) {
        fail("Voice response exceeds buffer", APP_ERR_VOICE_INTERNAL);
        return;
    }
    memcpy(s_message + s_message_used, event->data_ptr, event->data_len);
    s_message_used += event->data_len;
    if (event->fin && event->payload_offset + event->data_len == event->payload_len) {
        queue_message();
        s_message_used = 0;
        s_message_opcode = 0;
    }
}

/* ── 마이크 피드 (live_voice.c의 live_voice_feed()와 같다: 막지 않고, 밀리면 오래된 소리를 버린다) ── */

static void gemini_live_feed(const int16_t *samples, size_t count)
{
    if (!live_voice_ready() || voice_shared_stopping()) {
        return;
    }
    while (count) {
        size_t take = count > INPUT_FRAME_SAMPLES ? INPUT_FRAME_SAMPLES : count;
        input_frame_t frame = {.bytes = take * sizeof(int16_t)};
        memcpy(frame.samples, samples, frame.bytes);
        if (xQueueSend(s_inputs, &frame, 0) != pdTRUE) {
            input_frame_t stale;
            xQueueReceive(s_inputs, &stale, 0);
            xQueueSend(s_inputs, &frame, 0);
        }
        samples += take;
        count -= take;
    }
}

/* 최대 INPUT_BATCH_SAMPLES(64ms)까지 묶어 realtimeInput.audio 하나로 보낸다 */
static bool upload_audio(esp_websocket_client_handle_t client, input_frame_t *frame)
{
    int16_t *batch = s_batch;
    size_t samples = frame->bytes / sizeof(int16_t);
    memcpy(batch, frame->samples, frame->bytes);
    while (samples < INPUT_BATCH_SAMPLES && !voice_shared_stopping()) {
        if (xQueuePeek(s_inputs, frame, pdMS_TO_TICKS(20)) != pdTRUE ||
            frame->bytes / sizeof(int16_t) > INPUT_BATCH_SAMPLES - samples ||
            xQueueReceive(s_inputs, frame, 0) != pdTRUE) {
            break;
        }
        memcpy(batch + samples, frame->samples, frame->bytes);
        samples += frame->bytes / sizeof(int16_t);
    }
    char *text = gemini_live_build_audio_chunk(batch, samples);
    if (!text) {
        return false;
    }
    size_t n = strlen(text);
    bool ok = esp_websocket_client_send_text(client, text, n, pdMS_TO_TICKS(1000)) == (int)n;
    free(text);
    return ok;
}

/* ── 세션 ─────────────────────────────────────────────────── */

static bool wait_flag(atomic_bool *flag, uint32_t timeout_ms)
{
    for (uint32_t waited = 0; waited < timeout_ms && !atomic_load(flag) && !voice_shared_stopping(); waited += 100) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return atomic_load(flag);
}

static void reset_session_state(void)
{
    atomic_store(&s_ws_connected, false);
    atomic_store(&s_failed, false);
    atomic_store(&s_tools_total, 0);
    xQueueReset(s_inputs);
    audio_core_voice_clear();
    s_message_used = 0;
    s_message_opcode = 0;
    memset(s_cancelled_ids, 0, sizeof(s_cancelled_ids));
    s_cancelled_next = 0;
    saver_resample_reset(&s_resampler);
    hangup_phrase_reset(&s_hangup);
    for (int i = 0; i < ACT_COUNT; i++) {
        atomic_store(&s_act_count[i], 0);
    }
    xSemaphoreTake(s_pending_lock, portMAX_DELAY);
    free(s_pending_text);
    s_pending_text = NULL;
    xSemaphoreGive(s_pending_lock);
}

static void run_session(void)
{
    const live_voice_handlers_t *h = voice_shared_handlers();
    esp_websocket_client_handle_t client = NULL;
    esp_transport_handle_t tls = NULL, ws = NULL;
    const char *limit_reason = NULL;
    char *instructions = NULL, *setup_text = NULL;
    cJSON *gemini_tools = NULL;
    char *key = s_key, *headers = s_headers;

    reset_session_state();
    voice_shared_set_status("Connecting Wi-Fi");
    if (h->on_session_start) {
        h->on_session_start();
    }

    if (!wake_pause()) {
        fail("Wake detector did not release memory", APP_ERR_VOICE_INTERNAL);
        goto cleanup;
    }
    net_set_power_save(false);

    if (!net_wait_connected(WIFI_WAIT_MS)) {
        fail("Wi-Fi unavailable", APP_ERR_WIFI);
        goto cleanup;
    }
    voice_shared_set_status("Checking clock");
    for (uint32_t w = 0; w < CLOCK_WAIT_MS && !net_time_valid() && !voice_shared_stopping(); w += 100) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (!net_time_valid()) {
        fail("Clock sync failed", APP_ERR_CLOCK);
        goto cleanup;
    }
    if (voice_shared_stopping()) {
        goto cleanup;
    }

    size_t key_len = KEY_BUF_LEN - 1;
    if (secret_get(SECRET_GEMINI_KEY, key, &key_len) != ESP_OK || key_len == 0) {
        fail("Gemini API key not set", APP_ERR_NO_API_KEY);
        goto cleanup;
    }
    key[key_len] = '\0';
    snprintf(headers, HEADERS_BUF_LEN, "x-goog-api-key: %s\r\n", key);
    memset(key, 0, KEY_BUF_LEN);

    {
        char voice[32] = "";
        bool search = false;
        instructions = h->build_gemini_live_instructions
                           ? h->build_gemini_live_instructions(voice, sizeof(voice), &search)
                           : NULL;
        if (!instructions) {
            instructions = strdup("Answer briefly and naturally for speech.");
        }
        cJSON *tools = h->build_saver_tools ? h->build_saver_tools() : NULL;
        gemini_tools = gemini_schema_convert_tools(tools);   /* 같은 도구 목록을 Gemini 모양으로 (요청 4) */
        cJSON_Delete(tools);
        setup_text = gemini_live_build_setup(APP_GEMINI_LIVE_MODEL_DEFAULT, gemini_tts_voice(voice), instructions,
                                             gemini_tools, search);
    }
    if (!setup_text) {
        fail("Session startup failed", APP_ERR_VOICE_INTERNAL);
        goto cleanup;
    }

    tls = esp_transport_ssl_init();
    if (tls) {
        esp_transport_ssl_crt_bundle_attach(tls, esp_crt_bundle_attach);
        ws = esp_transport_ws_init(tls);
    }
    if (!ws) {
        fail("Could not allocate voice transport", APP_ERR_VOICE_INTERNAL);
        goto cleanup;
    }
    const esp_transport_ws_config_t ws_cfg = {
        .ws_path = GEMINI_LIVE_PATH,
        .headers = headers,
        .propagate_control_frames = true,
    };
    if (esp_transport_ws_set_config(ws, &ws_cfg) != ESP_OK) {
        fail("Could not configure voice transport", APP_ERR_VOICE_INTERNAL);
        goto cleanup;
    }
    const esp_websocket_client_config_t cfg = {
        .uri = GEMINI_LIVE_URI,
        .port = 443,
        .headers = headers,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .disable_auto_reconnect = true,
        .buffer_size = 4096,
        .task_stack = WS_TASK_STACK,
        .network_timeout_ms = 5000,
        .ext_transport = ws,
    };
    client = esp_websocket_client_init(&cfg);
    if (!client) {
        fail("Could not allocate voice connection", APP_ERR_VOICE_INTERNAL);
        goto cleanup;
    }
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event, tls);
    voice_shared_set_status("Connecting to Gemini");
    voice_shared_reserve_release();    /* 맡아 둔 내부 RAM 자리를 웹소켓 태스크 스택이 쓰게 한다 */
    if (esp_websocket_client_start(client) != ESP_OK || !wait_flag(&s_ws_connected, CONNECT_TIMEOUT_MS)) {
        fail("Gemini connection failed", APP_ERR_VOICE_CONNECT);
        goto cleanup;
    }
    if (esp_websocket_client_send_text(client, setup_text, strlen(setup_text), pdMS_TO_TICKS(2000)) !=
        (int)strlen(setup_text)) {
        fail("Session startup failed", APP_ERR_VOICE_INTERNAL);
        goto cleanup;
    }
    free(setup_text);
    setup_text = NULL;

    uint32_t began = now_ms();
    uint32_t max_ms, idle_ms;
    voice_shared_take_next_limits(&max_ms, &idle_ms);
    max_ms = max_ms ? max_ms : APP_VOICE_MAX_SESSION_MS;
    idle_ms = idle_ms ? idle_ms : APP_VOICE_IDLE_TIMEOUT_MS;
    ESP_LOGI(TAG, "limits: idle %u s, max %u s", (unsigned)(idle_ms / 1000), (unsigned)(max_ms / 1000));
    uint32_t act_log_ms = began;

    while (!voice_shared_stopping()) {
        if (h->on_tick) {
            h->on_tick();
        }
        uint32_t now = now_ms();
        if (!live_voice_ready() && now - began > STARTUP_TIMEOUT_MS) {
            fail("Voice startup timed out", APP_ERR_VOICE_CONNECT);
            break;
        }
        /* 무응답 시간: 실제로 들리는 비서 소리가 나가는 동안만 활동으로 본다 (LIVE와 같다. 책 16장 버그 2) */
        if (live_voice_ready() && audio_core_voice_playing()) {
            touch_activity(ACT_AUDIO);
        }
        if (live_voice_ready() && now - act_log_ms >= 10000) {
            act_log_ms = now;
            int32_t quiet = (int32_t)(now - atomic_load(&s_activity_ms));
            ESP_LOGI(TAG, "idle %d/%u s, last: %s | 10 s: heard %u, assistant text %u, audio %u, tool %u",
                     (int)(quiet > 0 ? quiet / 1000 : 0), (unsigned)(idle_ms / 1000), ACT_NAMES[atomic_load(&s_last_act)],
                     atomic_exchange(&s_act_count[ACT_IN_TEXT], 0), atomic_exchange(&s_act_count[ACT_OUT_TEXT], 0),
                     atomic_exchange(&s_act_count[ACT_AUDIO], 0), atomic_exchange(&s_act_count[ACT_TOOL], 0));
        }
        if (now - began > max_ms) {
            limit_reason = "Conversation limit reached";
        } else if (live_voice_ready() && (int32_t)(now - atomic_load(&s_activity_ms)) > (int32_t)idle_ms) {
            limit_reason = "Idle timeout";
        }
        if (limit_reason) {
            ESP_LOGI(TAG, "%s", limit_reason);
            voice_shared_set_status(limit_reason);
            live_voice_stop();
            break;
        }

        /* 기기가 보내는 글(알림·자동 읽기·마이크 보정 안내, 요청 5): clientContent로 */
        xSemaphoreTake(s_pending_lock, portMAX_DELAY);
        char *pending = s_pending_text;
        s_pending_text = NULL;
        xSemaphoreGive(s_pending_lock);
        if (pending) {
            char *msg = gemini_live_build_client_text(pending);
            free(pending);
            if (msg) {
                size_t n = strlen(msg);
                if (esp_websocket_client_send_text(client, msg, n, pdMS_TO_TICKS(1000)) != (int)n &&
                    !voice_shared_stopping()) {
                    fail("Device message failed", APP_ERR_VOICE_DROPPED);
                }
                free(msg);
            }
        }

        /* 도구 응답 (응답 해석 태스크가 넣어 둔 것) */
        char *out = NULL;
        if (xQueueReceive(s_outbound, &out, 0) == pdTRUE) {
            size_t n = strlen(out);
            if (esp_websocket_client_send_text(client, out, n, pdMS_TO_TICKS(1000)) != (int)n &&
                !voice_shared_stopping()) {
                fail("Device tool response failed", APP_ERR_VOICE_DROPPED);
            }
            free(out);
        }

        input_frame_t frame;
        if (xQueueReceive(s_inputs, &frame, pdMS_TO_TICKS(20)) == pdTRUE && live_voice_ready()) {
            if (!upload_audio(client, &frame) && !voice_shared_stopping()) {
                fail("Audio upload interrupted", APP_ERR_VOICE_DROPPED);
            }
        }
    }

cleanup:
    if (h->on_session_end) {
        h->on_session_end();
    }
    free(instructions);
    free(setup_text);
    cJSON_Delete(gemini_tools);
    voice_shared_set_ready(false);
    audio_core_voice_clear();
    if (client) {
        esp_websocket_client_stop(client);
        esp_websocket_client_destroy(client);
    }
    if (ws) {
        esp_transport_destroy(ws);
    }
    if (tls) {
        esp_transport_destroy(tls);
    }
    /* 해석 태스크가 남은 메시지를 다 처리할 때까지 기다린 뒤 다음 세션을 허용한다 (live_voice.c와 같다) */
    char *barrier = NULL;
    xQueueSend(s_responses, &barrier, portMAX_DELAY);
    xSemaphoreTake(s_responses_idle, portMAX_DELAY);
    char *stale = NULL;
    while (xQueueReceive(s_outbound, &stale, 0) == pdTRUE) {
        free(stale);
    }
    bool failed = atomic_load(&s_failed);
    unsigned tools_total = atomic_load(&s_tools_total);
    reset_session_state();
    memset(headers, 0, HEADERS_BUF_LEN);
    if (!failed && !limit_reason) {
        voice_shared_set_status("Ready");
    }
    ESP_LOGI(TAG, "session ended: %s (tools %u)", limit_reason ? limit_reason : (failed ? "error" : "stopped"),
             tools_total);
    net_set_power_save(true);
    voice_shared_reserve_take();    /* 호출어 모델을 다시 올리기 전에 자리를 맡는다 */
    wake_resume();
    voice_shared_end(true);
}

void gemini_live_task_entry(void *arg)
{
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        atomic_store(&s_session_active, true);
        run_session();
        atomic_store(&s_session_active, false);
    }
}

bool gemini_live_active(void)
{
    return atomic_load(&s_session_active);
}

esp_err_t gemini_live_text(const char *text)
{
    if (!atomic_load(&s_session_active) || !text || !text[0] || strlen(text) > 600) {
        return ESP_ERR_INVALID_STATE;
    }
    char *copy = strdup(text);
    if (!copy) {
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreTake(s_pending_lock, portMAX_DELAY);
    free(s_pending_text);
    s_pending_text = copy;
    xSemaphoreGive(s_pending_lock);
    const live_voice_handlers_t *h = voice_shared_handlers();
    if (h && h->on_user_input) {
        h->on_user_input();     /* 글로 보낸 요청도 사용자 입력이다 (live_voice_text()와 같다) */
    }
    return ESP_OK;
}

esp_err_t gemini_live_init(void)
{
    static StaticQueue_t input_ctrl;
    uint8_t *input_storage = heap_caps_malloc(INPUT_QUEUE_FRAMES * sizeof(input_frame_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_message = heap_caps_malloc(MESSAGE_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_cancelled_ids = heap_caps_calloc(CANCELLED_ID_MAX, CANCELLED_ID_LEN, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_batch = heap_caps_malloc(INPUT_BATCH_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_key = heap_caps_calloc(1, KEY_BUF_LEN, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_headers = heap_caps_calloc(1, HEADERS_BUF_LEN, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!input_storage || !s_message || !s_cancelled_ids || !s_batch || !s_key || !s_headers) {
        return ESP_ERR_NO_MEM;
    }
    s_inputs = xQueueCreateStatic(INPUT_QUEUE_FRAMES, sizeof(input_frame_t), input_storage, &input_ctrl);
    s_responses = xQueueCreateWithCaps(16, sizeof(char *), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_outbound = xQueueCreateWithCaps(OUTBOUND_QUEUE, sizeof(char *), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_responses_idle = xSemaphoreCreateBinary();
    s_pending_lock = xSemaphoreCreateMutex();
    if (!s_inputs || !s_responses || !s_outbound || !s_responses_idle || !s_pending_lock) {
        return ESP_ERR_NO_MEM;
    }
    ESP_ERROR_CHECK(audio_core_add_mic_sink(gemini_live_feed));
    /* 응답 해석·재생 큐 넣기·도구 실행 → 스택은 PSRAM (XIP from PSRAM이라 도구의 플래시 쓰기도 됨, tool_worker와 같음).
     * 도구(라디오 검색 HTTPS 등)를 이 태스크에서 바로 실행하므로 tool_worker와 같은 12KB (8KB면 넘칠 수 있음) */
    if (xTaskCreatePinnedToCoreWithCaps(response_task, "gemini_rx", 12288, NULL, 5, NULL, 0,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
