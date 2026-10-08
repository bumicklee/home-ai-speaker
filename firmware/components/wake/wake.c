/* Based on waveshare-ai-speaker-assistant (MIT, Copyright (c) 2026 KMX415). See THIRD_PARTY_NOTICES.md. */
/*
 * 기존 펌웨어 wake_word.c 이식. 바뀐 점:
 * - live_voice·LED·USB를 직접 부르지 않고, 감지 결과를 APP_EVT_WAKE_DETECTED로 보낸다
 * - 설정은 settings 모듈(NVS wake 네임스페이스)에 저장하고 메모리에 캐시한다
 * - 대화 중 일시정지는 wake_pause/wake_resume으로 명시적으로 요청받는다
 */
#include "wake.h"

#include <stdatomic.h>
#include <string.h>
#include "app_config.h"
#include "app_events.h"
#include "audio_core.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "model_path.h"

static const char *TAG = "WAKE";

#define FRAME_SAMPLES  256
#define QUEUE_FRAMES   32     /* 약 0.5초 (PSRAM) */
#define STATS_MS       60000  /* 진단 로그 주기 (튜닝 끝나면 1분) */
#define COOLDOWN_MS    3000
#define TEST_MS        60000
/* 발화 진단: 호출어 입력(증폭 후)이 이보다 크면 "소리 들림", 이만큼 조용하면 끝 */
#define UTTER_ON       3000
#define UTTER_OFF      1000
#define UTTER_GAP_MS   600
#define UTTER_MIN_MS   200    /* 짧은 딸깍 소리는 남기지 않는다 (콘솔 입력을 덜 방해하게) */

typedef struct {
    size_t count;
    int16_t samples[FRAME_SAMPLES];
} wake_frame_t;

static settings_wake_t s_cfg;
static SemaphoreHandle_t s_lock;
static QueueHandle_t s_audio;
static atomic_bool s_running;           /* wake_start ~ wake_stop */
static atomic_bool s_available;         /* 모델이 올라가 듣는 중 */
static atomic_bool s_pause_requested, s_pause_done;
static atomic_bool s_task_alive;        /* 모델을 못 올려 태스크가 끝났으면 false */
static atomic_uint s_test_until;
static atomic_uint s_detections, s_dropped;
static atomic_uint s_fed_peak;          /* 게인을 준 뒤 최대 진폭 (진단) */
static atomic_int s_gain_q8 = 256 * 4;  /* 디지털 게인 (Q8, 256 = 1배). wake_set_config가 갱신 */

static int gain_q8_from_db(int db)
{
    /* 10^(db/20) × 256 — 3dB 단위 표로 (부동소수점 없이) */
    static const int table[] = {256, 362, 511, 722, 1020, 1441, 2035, 2875, 4062};   /* 0,3,…,24 dB */
    db = db < 0 ? 0 : (db > 24 ? 24 : db);
    return table[db / 3];
}

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static settings_wake_t snapshot(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    settings_wake_t c = s_cfg;
    xSemaphoreGive(s_lock);
    return c;
}

bool wake_is_testing(void)
{
    uint32_t until = atomic_load(&s_test_until);
    return until && (int32_t)(until - now_ms()) > 0;
}

bool wake_is_listening(void)
{
    return atomic_load(&s_available);
}

esp_err_t wake_set_config(const settings_wake_t *cfg)
{
    esp_err_t err = settings_set_wake(cfg);
    if (err == ESP_OK) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_cfg = *cfg;
        xSemaphoreGive(s_lock);
        atomic_store(&s_gain_q8, gain_q8_from_db(cfg->gain_db));
    }
    return err;
}

void wake_get_config(settings_wake_t *out)
{
    *out = snapshot();
}

esp_err_t wake_test(bool enable)
{
    atomic_store(&s_test_until, enable ? now_ms() + TEST_MS : 0);
    return ESP_OK;
}

bool wake_pause(void)
{
    if (!atomic_load(&s_task_alive)) {
        return true;    /* 쥐고 있는 메모리가 없다 */
    }
    atomic_store(&s_pause_done, false);
    atomic_store(&s_pause_requested, true);
    for (int i = 0; i < 500 && !atomic_load(&s_pause_done); i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return atomic_load(&s_pause_done);
}

void wake_resume(void)
{
    atomic_store(&s_pause_requested, false);
}

esp_err_t wake_start(void)
{
    atomic_store(&s_running, true);
    return ESP_OK;
}

esp_err_t wake_stop(void)
{
    atomic_store(&s_running, false);
    return ESP_OK;
}

/* 마이크 태스크에서 불린다. 막지 않고, 큐가 차면 버린다 */
static void wake_feed(const int16_t *samples, size_t count)
{
    if (!atomic_load(&s_available)) {
        return;
    }
    while (count) {
        size_t n = count > FRAME_SAMPLES ? FRAME_SAMPLES : count;
        wake_frame_t f = {.count = n};
        /*
         * 호출어 입력에만 디지털 게인을 준다. 실기기에서 "Jarvis"를 말할 때 최대 진폭이 500–1,100
         * (최대의 2–3%)뿐이라 WakeNet이 감지하지 못했다 (책 6장 버그 4). 대화 업로드는 그대로 둔다.
         * 넘치는 값은 잘라 낸다(찌그러짐보다 놓침이 더 나쁘다).
         */
        int g = atomic_load(&s_gain_q8);
        unsigned peak = atomic_load(&s_fed_peak);
        for (size_t i = 0; i < n; i++) {
            int v = (samples[i] * g) >> 8;
            v = v > 32767 ? 32767 : (v < -32768 ? -32768 : v);
            f.samples[i] = (int16_t)v;
            unsigned a = v < 0 ? -v : v;
            if (a > peak) {
                peak = a;
            }
        }
        atomic_store(&s_fed_peak, peak);
        if (xQueueSend(s_audio, &f, 0) != pdTRUE) {
            atomic_fetch_add(&s_dropped, 1);
            return;
        }
        samples += n;
        count -= n;
    }
}

static srmodel_list_t *s_models;

/*
 * 스택이 PSRAM이라 플래시 작업(NVS·파티션 매핑)을 하면 안 된다 (책 7장 7.3절).
 * 모델 목록은 wake_init()이 main 태스크에서 미리 연다.
 */
static void wake_task(void *arg)
{
    srmodel_list_t *models = s_models;
    const esp_wn_iface_t *engine = NULL;
    model_iface_data_t *model = NULL;
    int loaded = -1, sensitivity = -1, chunk = 0;
    size_t used = 0;
    int16_t *buffer = NULL;
    uint32_t cooldown = now_ms() + COOLDOWN_MS, seen_dropped = 0;
    /* 진단: 실시간을 따라가는지 (chunk 하나 처리 시간 < chunk 길이여야 한다) */
    uint32_t stat_since = now_ms(), stat_frames = 0, stat_detects = 0, stat_max_us = 0;
    uint64_t stat_total_us = 0;
    /* 발화 하나(소리가 났다 그친 구간)마다 "감지 안 됨"을 남긴다. 말했는데 못 알아들을 때 크기·길이를 본다 */
    bool utter = false;
    uint32_t utter_start = 0, utter_last = 0;
    int utter_peak = 0;
    unsigned stat_queue_max = 0;

    for (;;) {
        settings_wake_t cfg = snapshot();
        bool testing = wake_is_testing();
        bool paused = atomic_load(&s_pause_requested) || !atomic_load(&s_running) || (!cfg.enabled && !testing);
        if (paused) {
            /* 모델을 내려 메모리를 돌려준다 (대화 중 TLS용) */
            atomic_store(&s_available, false);
            if (model) {
                engine->destroy(model);
                model = NULL;
            }
            free(buffer);
            buffer = NULL;
            loaded = -1;
            used = 0;
            xQueueReset(s_audio);
            if (atomic_load(&s_pause_requested)) {
                atomic_store(&s_pause_done, true);
            }
            cooldown = now_ms() + COOLDOWN_MS;
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (loaded != cfg.phrase) {
            atomic_store(&s_available, false);
            if (model) {
                engine->destroy(model);
                model = NULL;
            }
            free(buffer);
            buffer = NULL;
            const char *name = esp_srmodel_filter(models, "wn9", cfg.phrase ? "computer" : "alexa");
            engine = name ? esp_wn_handle_from_name(name) : NULL;
            if (engine) {
                model = engine->create(name, DET_MODE_90);
            }
            if (!model) {
                ESP_LOGE(TAG, "wake model failed to load");
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
            chunk = engine->get_samp_chunksize(model);
            if (chunk <= 0 || chunk > 4096 || engine->get_samp_rate(model) != AUDIO_SAMPLE_RATE ||
                engine->get_channel_num(model) != 1) {
                engine->destroy(model);
                model = NULL;
                ESP_LOGE(TAG, "unsupported wake model");
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
            buffer = heap_caps_aligned_alloc(16, chunk * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (!buffer) {
                engine->destroy(model);
                model = NULL;
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
            loaded = cfg.phrase;
            sensitivity = -1;
            used = 0;
            xQueueReset(s_audio);
            atomic_store(&s_available, true);
            app_events_post(APP_EVT_WAKE_READY, NULL, 0, 0);
            ESP_LOGI(TAG, "listening for \"%s\" (sensitivity %d, input gain +%d dB)", cfg.phrase ? "Computer" : "Alexa",
                     cfg.sensitivity, cfg.gain_db);
        }

        if (sensitivity != cfg.sensitivity) {
            /* 50%가 제조사 기본 문턱값. 높을수록 민감하다 */
            engine->reset_det_threshold(model);
            float threshold = engine->get_det_threshold(model, 1) + (50 - cfg.sensitivity) * 0.004f;
            threshold = threshold < 0.4f ? 0.4f : (threshold > 0.95f ? 0.95f : threshold);
            engine->set_det_threshold(model, threshold, 1);
            used = 0;
            sensitivity = cfg.sensitivity;
        }

        if (now_ms() - stat_since >= STATS_MS) {
            unsigned dropped = atomic_load(&s_dropped);
            ESP_LOGI(TAG, "stats: frames %u, detect %u (avg %u us, max %u us, chunk %d = %d ms), dropped %u, queue max %u, peak %u",
                     (unsigned)stat_frames, (unsigned)stat_detects,
                     stat_detects ? (unsigned)(stat_total_us / stat_detects) : 0, (unsigned)stat_max_us, chunk,
                     chunk * 1000 / AUDIO_SAMPLE_RATE, dropped, stat_queue_max, atomic_exchange(&s_fed_peak, 0));
            stat_since = now_ms();
            stat_frames = stat_detects = stat_max_us = 0;
            stat_total_us = 0;
            stat_queue_max = 0;
        }
        wake_frame_t f;
        if (xQueueReceive(s_audio, &f, pdMS_TO_TICKS(100)) != pdTRUE) {
            continue;
        }
        stat_frames++;
        unsigned depth = uxQueueMessagesWaiting(s_audio);
        if (depth > stat_queue_max) {
            stat_queue_max = depth;
        }
        /* 프레임을 잃었으면 만들던 조각만 버린다 (큐에 쌓인 소리는 살린다) */
        if (seen_dropped != atomic_load(&s_dropped)) {
            seen_dropped = atomic_load(&s_dropped);
            used = 0;
        }
        for (size_t off = 0; off < f.count;) {
            size_t n = chunk - used;
            if (n > f.count - off) {
                n = f.count - off;
            }
            memcpy(buffer + used, f.samples + off, n * sizeof(int16_t));
            used += n;
            off += n;
            if (used != (size_t)chunk) {
                continue;
            }
            used = 0;
            int chunk_peak = 0;
            for (int i = 0; i < chunk; i++) {
                int a = buffer[i] < 0 ? -buffer[i] : buffer[i];
                chunk_peak = a > chunk_peak ? a : chunk_peak;
            }
            uint32_t t = now_ms();
            if (chunk_peak >= UTTER_ON && !audio_core_is_playing()) {
                if (!utter) {
                    utter = true;
                    utter_start = t;
                    utter_peak = 0;
                }
                utter_last = t;
                utter_peak = chunk_peak > utter_peak ? chunk_peak : utter_peak;
            } else if (utter && chunk_peak < UTTER_OFF && t - utter_last >= UTTER_GAP_MS) {
                utter = false;
                unsigned ms = (unsigned)(utter_last - utter_start + 32);
                if (ms >= UTTER_MIN_MS) {
                    ESP_LOGI(TAG, "sound %u ms, peak %d%s: not detected", ms, utter_peak,
                             utter_peak >= 32700 ? " (clipped)" : "");
                }
            }
            int64_t t0 = esp_timer_get_time();
            int detected = engine->detect(model, buffer);
            uint32_t us = (uint32_t)(esp_timer_get_time() - t0);
            stat_detects++;
            stat_total_us += us;
            if (us > stat_max_us) {
                stat_max_us = us;
            }
            if (detected > 0 && (int32_t)(now_ms() - cooldown) >= 0) {
                cooldown = now_ms() + COOLDOWN_MS;
                xQueueReset(s_audio);
                utter = false;      /* 감지됐으니 이 발화는 로그하지 않는다 */
                unsigned count = atomic_fetch_add(&s_detections, 1) + 1;
                ESP_LOGI(TAG, "wake word detected (%u)", count);
                if (wake_is_testing()) {
                    app_events_post(APP_EVT_WAKE_TEST, &count, sizeof(count), 0);
                } else {
                    app_events_post(APP_EVT_WAKE_DETECTED, NULL, 0, 0);
                }
                break;
            }
        }
    }
}

esp_err_t wake_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    static StaticQueue_t queue_ctrl;
    uint8_t *storage = heap_caps_malloc(QUEUE_FRAMES * sizeof(wake_frame_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_lock || !storage) {
        return ESP_ERR_NO_MEM;
    }
    s_audio = xQueueCreateStatic(QUEUE_FRAMES, sizeof(wake_frame_t), storage, &queue_ctrl);
    settings_get_wake(&s_cfg);
    atomic_store(&s_gain_q8, gain_q8_from_db(s_cfg.gain_db));
    s_models = esp_srmodel_init(APP_PART_MODEL);
    if (!s_models) {
        ESP_LOGE(TAG, "wake models unavailable; Key2 still starts a conversation");
        return ESP_OK;      /* 태스크 없음: wake_pause()는 바로 성공한다 */
    }
    audio_core_set_wake_sink(wake_feed);    /* 조용할 때는 원본 마이크, 스피커가 소리를 내면 AEC 출력 */
    atomic_store(&s_task_alive, true);
    return xTaskCreatePinnedToCoreWithCaps(wake_task, "wake", 12288, NULL, 4, NULL, 0,
                                           MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
