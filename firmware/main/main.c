/**
 * @file main.c
 * @brief 부팅 흐름과 시스템 상태머신 (설계서 2.4, 2.5, 2.15절)
 *
 * 이벤트(app_events)를 받아 LED 표시를 바꾸고, 버튼·호출어에 따라 대화를 시작한다.
 * 하위 모듈은 main을 모른다. main만 모든 모듈을 안다.
 */
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "app_config.h"
#include "app_events.h"
#include "assistant.h"
#include "audio_core.h"
#include "ble_prov.h"
#include "device_api.h"
#include "board.h"
#include "dev_console.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "live_voice.h"
#include "local_api.h"
#include "media_radio.h"
#include "media_spotify.h"
#include "messenger_telegram.h"
#include "net.h"
#include "notes.h"
#include "secret_store.h"
#include "settings.h"
#include "wake.h"

static const char *TAG = "MAIN";

typedef enum {
    SYS_MODE_NORMAL,
    SYS_MODE_PROVISIONING,
    SYS_MODE_OTA,
    SYS_MODE_FACTORY_RESET,
} sys_mode_t;

static sys_mode_t s_mode = SYS_MODE_NORMAL;
static bool s_app_marked_valid;
/* 준비 안내 (부팅마다 한 번): 호출어 듣기 + Wi-Fi + OpenAI 키 → "AI 스피커가 준비되었습니다" */
static bool s_wifi_ready, s_wake_ready, s_ready_announced;

/*
 * BLE 창을 열 때 내부 RAM 비우기 (책 8장 버그 1): 대화를 끝내고 AEC(약 91KB)를 내린다.
 * 창이 열린 동안은 대화를 시작할 수 없다(live_voice_start가 거절). 라디오는 계속 나온다.
 */
static void ble_before_open(void)
{
    if (live_voice_active()) {
        live_voice_stop();
        for (int i = 0; i < 50 && live_voice_active(); i++) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    if (audio_core_aec_suspend(true, 2000) != ESP_OK) {
        ESP_LOGW(TAG, "AEC suspend timed out");
    }
    ESP_LOGI(TAG, "BLE window: internal free %u (largest %u)",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
}

static void ble_after_close(void)
{
    if (audio_core_aec_suspend(false, 3000) != ESP_OK) {
        ESP_LOGW(TAG, "AEC resume pending (will retry)");
    }
}

static void announce_ready_if_ready(void)
{
    settings_wake_t w;
    wake_get_config(&w);
    bool wake_ok = s_wake_ready || !w.enabled;      /* 호출어를 꺼 두었으면 Key2로 대화할 수 있으면 준비 */
    if (s_ready_announced || !s_wifi_ready || !wake_ok || s_mode != SYS_MODE_NORMAL || live_voice_active()) {
        return;
    }
    s_ready_announced = true;
    if (!secret_exists(SECRET_OPENAI_KEY)) {
        ESP_LOGI(TAG, "ready, but no OpenAI key: no announcement");
        return;
    }
    ESP_LOGI(TAG, "ready: announcing");
    audio_core_play_ready();
}

/* ── 시간 ─────────────────────────────────────────────────── */

/* 단계별 내부 RAM 여유 (어디서 얼마나 쓰는지 보려고) */
static void log_heap(const char *step)
{
    ESP_LOGI(TAG, "heap after %-12s internal %6u (largest %6u)  psram %7u", step,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

/* 없어도 대화는 되는 기능: 실패하면 그 기능만 끄고 계속 부팅한다 (재부팅 반복 방지) */
static void optional_init(const char *name, esp_err_t err)
{
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s init failed: %s (feature disabled)", name, esp_err_to_name(err));
    }
    log_heap(name);
}

static void restore_time_from_rtc(void)
{
    struct tm utc;
    if (!board_rtc_read(&utc)) {
        ESP_LOGI(TAG, "RTC not set; waiting for SNTP");
        return;
    }
    /* RTC는 UTC다. 현지 시간대(assistant의 timezone)가 설정돼 있어도 UTC로 계산한다 */
    char *saved = getenv("TZ") ? strdup(getenv("TZ")) : NULL;
    setenv("TZ", "UTC0", 1);
    tzset();
    struct timeval tv = {.tv_sec = mktime(&utc)};
    if (saved) {
        setenv("TZ", saved, 1);
        free(saved);
    } else {
        unsetenv("TZ");
    }
    tzset();
    settimeofday(&tv, NULL);
    ESP_LOGI(TAG, "time restored from RTC");
}

static void on_time_sync(void)
{
    time_t now = time(NULL);
    struct tm utc;
    gmtime_r(&now, &utc);
    board_rtc_write(&utc);
}

/* ── 이벤트 처리 ──────────────────────────────────────────── */

static void change_volume(int delta)
{
    int v = audio_core_get_volume() + delta;
    v = v < 0 ? 0 : (v > APP_VOLUME_MAX ? APP_VOLUME_MAX : v);
    if (v != audio_core_get_volume()) {
        audio_core_set_volume(v);
        ESP_LOGI(TAG, "volume %d", v);
    }
}

static void factory_reset(void)
{
    s_mode = SYS_MODE_FACTORY_RESET;
    ESP_LOGW(TAG, "factory reset");
    live_voice_stop();
    wake_stop();
    radio_stop();
    settings_factory_reset();      /* 휴대폰 토큰(api/phones)도 nvs에 있어 함께 지워진다 */
    radio_presets_reset();         /* storage의 /presets.json을 기본값으로 (SYS-08) */
    vTaskDelay(pdMS_TO_TICKS(1500));    /* 빨간 깜빡임을 잠깐 보여 준다 */
    esp_restart();
}

static void on_button(const app_button_event_t *evt)
{
    if (s_mode == SYS_MODE_FACTORY_RESET) {
        return;
    }
    switch (evt->button) {
    case APP_BUTTON_KEY1:
        change_volume(-APP_VOLUME_STEP);
        break;
    case APP_BUTTON_KEY3:
        change_volume(APP_VOLUME_STEP);
        break;
    case APP_BUTTON_KEY2:
        switch (evt->press) {
        case APP_PRESS_SHORT:
            /* 음악 재생 중에도 대화 시작, 음악은 덕킹 (Q5 사용자 결정 2026-10-03).
             * 절약 모드이고 지금 말하는 중이면 재생만 멈추고 계속 듣는다(대화는 안 끝남, 끼어들기,
             * docs/design-voice-saver-mode.md 11.5). 그 밖에는 지금처럼 대화를 끝낸다 */
            if (live_voice_active()) {
                live_voice_barge_in();
            } else if (s_mode == SYS_MODE_NORMAL) {
                live_voice_start();
            }
            break;
        case APP_PRESS_HOLD_3S:
            led_ring_set_key2_stage(LED_KEY2_BLE);
            break;
        case APP_PRESS_HOLD_7S:
            led_ring_set_key2_stage(LED_KEY2_WARN);
            break;
        case APP_PRESS_BLE_WINDOW:
            led_ring_set_key2_stage(LED_KEY2_NONE);
            ble_prov_open_window();
            break;
        case APP_PRESS_CANCEL:
            led_ring_set_key2_stage(LED_KEY2_NONE);
            break;
        case APP_PRESS_FACTORY:
            led_ring_set_key2_stage(LED_KEY2_FACTORY);
            factory_reset();
            break;
        default:
            break;
        }
        break;
    }
}

/*
 * 새 Telegram 메시지 자동 읽기 (TLG-10). 앱의 "새 메시지 바로 읽어 주기"가 켜져 있고 조용한 시간이 아니면
 * 짧은 대화를 열어 읽어 준다. 대화·영어 수업·BLE 창 때문에 못 열면 5초마다 다시 시도한다.
 * 조용한 시간에 온 메시지는 지금처럼 알림음·보라색 LED로만 알린다.
 */
static bool s_tg_pending;
static esp_timer_handle_t s_tg_retry;
static const char *s_tg_wait_reason;   /* 같은 이유를 5초마다 찍지 않게 */

/* 기다려야 할 때 5초 뒤에 다시 시도한다 (타이머 태스크 → 이벤트 루프로 넘겨 같은 태스크에서 처리) */
static void tg_retry_cb(void *arg)
{
    int unread = telegram_unread();
    app_events_post(APP_EVT_TG_MESSAGE, &unread, sizeof(unread), 0);
}

static void tg_retry_later(const char *reason)
{
    if (reason != s_tg_wait_reason) {
        s_tg_wait_reason = reason;
        ESP_LOGI(TAG, "telegram: auto-read waiting (%s)", reason);
    }
    if (!s_tg_retry) {
        const esp_timer_create_args_t args = {.callback = tg_retry_cb, .name = "tg_retry"};
        if (esp_timer_create(&args, &s_tg_retry) != ESP_OK) {
            return;
        }
    }
    if (!esp_timer_is_active(s_tg_retry)) {
        esp_timer_start_once(s_tg_retry, 5 * 1000 * 1000);
    }
}

static void auto_read_telegram(void)
{
    if (!s_tg_pending) {
        return;
    }
    int unread = telegram_unread();
    if (unread <= 0 || !telegram_auto_read_now()) {
        s_tg_pending = false;   /* 이미 읽었거나, 꺼져 있거나, 조용한 시간 (이유는 TG_STORE 로그) */
        return;
    }
    if (!secret_exists(SECRET_OPENAI_KEY)) {
        s_tg_pending = false;
        ESP_LOGW(TAG, "telegram: auto-read needs the OpenAI API key");
        return;
    }
    if (live_voice_active()) {
        tg_retry_later("conversation in progress");
        return;
    }
    if (ble_prov_is_open() || audio_core_aec_suspended()) {
        tg_retry_later("BLE window open");
        return;
    }
    if (wake_is_testing()) {
        tg_retry_later("wake word test");
        return;
    }
    esp_err_t err = assistant_announce_telegram(unread);
    if (err == ESP_OK) {
        s_tg_pending = false;
        s_tg_wait_reason = NULL;
        ESP_LOGI(TAG, "telegram: auto-read %d", unread);
    } else if (err == ESP_ERR_INVALID_STATE) {
        tg_retry_later("busy (English lesson or starting)");
    } else {
        s_tg_pending = false;
        ESP_LOGW(TAG, "telegram: auto-read failed: %s", esp_err_to_name(err));
    }
}

/*
 * 일정 알림 (docs/design-notes-schedule.md 2판 5.4절): notes_tick()이 묶음을 들고 있으면 짧은 대화를 연다.
 * 대화·영어 수업·BLE 창·호출어 시험 때문에 못 열면 텔레그램 자동 읽기와 같은 5초 재시도.
 * 인터넷·API 키가 없어 애초에 될 수 없으면(일시적이지 않음) 알림음 3번 + 주황 LED로 비상 알림.
 * 같은 순간에 텔레그램 자동 읽기도 기다리고 있으면 알림이 먼저(on_app_event의 호출 순서로 보장).
 */
static bool s_notes_pending;
static esp_timer_handle_t s_notes_retry;
static const char *s_notes_wait_reason;

static void notes_retry_cb(void *arg)
{
    app_events_post(APP_EVT_NOTES_TICK, NULL, 0, 0);
}

static void notes_retry_later(const char *reason)
{
    if (reason != s_notes_wait_reason) {
        s_notes_wait_reason = reason;
        ESP_LOGI(TAG, "reminder: waiting (%s)", reason);
    }
    if (!s_notes_retry) {
        const esp_timer_create_args_t args = {.callback = notes_retry_cb, .name = "notes_retry"};
        if (esp_timer_create(&args, &s_notes_retry) != ESP_OK) {
            return;
        }
    }
    if (!esp_timer_is_active(s_notes_retry)) {
        esp_timer_start_once(s_notes_retry, 5 * 1000 * 1000);
    }
}

static esp_timer_handle_t s_reminder_chime_timer;
static int s_reminder_chime_left;

static void reminder_chime_cb(void *arg)
{
    audio_core_chime();
    if (--s_reminder_chime_left > 0) {
        esp_timer_start_once(s_reminder_chime_timer, 1200 * 1000);
    }
}

/* 대화를 열 수 없음(인터넷·API 키 없음): 알림음 3번(겹치지 않게 esp_timer로 띄엄띄엄) + 주황 LED.
 * notes 쪽은 묶음을 missed로 남기고 비운다(2026-10-04 결정: 단순하게, 30분 안에 인터넷이 와도 다시 말로 알리지 않는다) */
static void reminder_emergency(void)
{
    led_ring_set_reminder(true);
    if (!s_reminder_chime_timer) {
        const esp_timer_create_args_t args = {.callback = reminder_chime_cb, .name = "rem_chime"};
        esp_timer_create(&args, &s_reminder_chime_timer);
    }
    if (s_reminder_chime_timer) {
        s_reminder_chime_left = 3;
        reminder_chime_cb(NULL);       /* 첫 번째는 바로, 나머지 둘은 1.2초 간격으로 스스로 이어진다 */
    }
    notes_reminder_unreachable();
    ESP_LOGW(TAG, "reminder: no Wi-Fi or API key; chime + LED instead of talking");
}

static void process_reminders(void)
{
    if (!s_notes_pending) {
        return;
    }
    if (!secret_exists(SECRET_OPENAI_KEY) || !net_is_connected()) {
        reminder_emergency();
        s_notes_pending = false;
        s_notes_wait_reason = NULL;
        return;
    }
    if (live_voice_active()) {
        notes_retry_later("conversation in progress");
        return;
    }
    if (ble_prov_is_open() || audio_core_aec_suspended()) {
        notes_retry_later("BLE window open");
        return;
    }
    if (wake_is_testing()) {
        notes_retry_later("wake word test");
        return;
    }
    esp_err_t err = assistant_announce_reminders();
    if (err == ESP_OK) {
        s_notes_pending = false;
        s_notes_wait_reason = NULL;
        ESP_LOGI(TAG, "reminder: announcing");
    } else if (err == ESP_ERR_INVALID_STATE) {
        notes_retry_later("busy (English lesson or starting)");
    } else {
        reminder_emergency();      /* 예상 밖의 실패도 인터넷·키 문제로 본다 */
        s_notes_pending = false;
        s_notes_wait_reason = NULL;
    }
}

static void on_app_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    switch ((app_event_id_t)id) {
    case APP_EVT_WIFI_UP:
        led_ring_set_wifi_connecting(false);
        s_wifi_ready = true;
        announce_ready_if_ready();
        /* OTA로 받은 새 펌웨어: 오디오와 Wi-Fi가 올라왔으면 정상으로 확정 (2.18절) */
        if (!s_app_marked_valid) {
            s_app_marked_valid = true;
            esp_ota_mark_app_valid_cancel_rollback();
        }
        break;
    case APP_EVT_WAKE_READY:
        s_wake_ready = true;
        announce_ready_if_ready();
        break;
    case APP_EVT_WIFI_DOWN:
        led_ring_set_wifi_connecting(true);
        break;
    case APP_EVT_PROV_START:
        led_ring_set_ble_window(true);
        break;
    case APP_EVT_PROV_DONE:
        led_ring_set_ble_window(false);
        break;
    case APP_EVT_WAKE_DETECTED:
        led_ring_flash_wake();
        audio_core_music_duck(true);    /* 말을 알아듣기 쉽게 바로 줄인다 (2.5절) */
        if (s_mode == SYS_MODE_NORMAL && !live_voice_active()) {
            live_voice_start();
        }
        break;
    case APP_EVT_VOICE_STATE: {
        voice_state_t v = *(const voice_state_t *)data;
        led_ring_set_voice(v);
        if (v != VOICE_IDLE) {
            led_ring_set_reminder(false);   /* 다음 대화가 시작됐다: 비상 알림 표시를 끈다 */
        }
        /* 대화하는 동안 음악을 덕킹, 끝나면 복원 (Spotify 일시정지 옵션은 Phase 5) */
        audio_core_music_duck(v != VOICE_IDLE);
        /* 대화 중에는 Telegram 폴링을 멈춘다 (TLG-12, TLS 연결 하나를 아낀다) */
        telegram_pause(v != VOICE_IDLE);
        if (v == VOICE_IDLE && radio_is_active()) {
            net_set_power_save(false);  /* live_voice가 대화 끝에 절전을 켰지만 음악이 아직 나온다 */
        }
        if (v == VOICE_IDLE) {
            process_reminders();        /* 일정 알림이 먼저(기다리던 것 다시 시도) */
            auto_read_telegram();       /* 대화 중에 와서 미뤄 둔 메시지 */
        }
        break;
    }
    case APP_EVT_NOTES_TICK:
        notes_tick();
        if (notes_reminder_pending()) {
            s_notes_pending = true;
        }
        process_reminders();
        break;
    case APP_EVT_TG_MESSAGE: {
        static int last_unread;
        int unread = *(const int *)data;
        led_ring_set_unread(unread > 0);
        /* 새 메시지: 알림음. 대화 중에는 생략 (TLG-07) */
        if (unread > last_unread && !live_voice_active()) {
            audio_core_chime();
        }
        if (unread > last_unread) {
            s_tg_pending = true;
            s_tg_wait_reason = NULL;
        }
        auto_read_telegram();   /* 새 메시지, 또는 기다리던 것 다시 시도 (tg_retry_cb) */
        last_unread = unread;
        break;
    }
    case APP_EVT_MEDIA_STATE:
        led_ring_set_media(((const app_media_event_t *)data)->state);
        /* 음악이 나오는 동안은 Wi-Fi 절전을 끈다 (2.19절) */
        net_set_power_save(((const app_media_event_t *)data)->state == MEDIA_STOPPED ||
                           ((const app_media_event_t *)data)->state == MEDIA_PAUSED);
        break;
    case APP_EVT_BUTTON:
        on_button(data);
        break;
    case APP_EVT_ERROR:
        if (*(const app_error_t *)data != APP_ERR_NONE) {
            led_ring_show_error();
        }
        break;
    default:
        break;
    }
}

/* 호출어 대기 표시는 wake 모듈 상태를 0.5초마다 반영한다 */
static void wake_led_timer(void *arg)
{
    led_ring_set_wake_armed(wake_is_listening());
}

static void start_led_timer(void)
{
    const esp_timer_create_args_t args = {.callback = wake_led_timer, .name = "wake_led"};
    esp_timer_handle_t timer;
    if (esp_timer_create(&args, &timer) == ESP_OK) {
        esp_timer_start_periodic(timer, 500 * 1000);
    }
}

/* 30초마다 notes 모듈에게 알릴 것·놓친 것을 검사하라고 알린다 (새 태스크 없이 이벤트 루프에서, 2.3절) */
static void notes_tick_cb(void *arg)
{
    app_events_post(APP_EVT_NOTES_TICK, NULL, 0, 0);
}

static void start_notes_timer(void)
{
    const esp_timer_create_args_t args = {.callback = notes_tick_cb, .name = "notes_tick"};
    esp_timer_handle_t timer;
    if (esp_timer_create(&args, &timer) == ESP_OK) {
        esp_timer_start_periodic(timer, 30 * 1000 * 1000);
    }
}

/* ── 부팅 ─────────────────────────────────────────────────── */

static void start_services(void)
{
    led_ring_set_wifi_connecting(true);
    if (net_start() != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi not started");
    }
    net_mdns_start();
    optional_init("local_api", local_api_start());
    ESP_ERROR_CHECK(wake_start());
    ESP_ERROR_CHECK(telegram_start());      /* Phase 4 */
}

void app_main(void)
{
    ESP_LOGI(TAG, "AI Speaker %s (protocol %d)", APP_FW_VERSION, APP_PROTOCOL_VERSION);

    log_heap("start");
    ESP_ERROR_CHECK(settings_init());       /* nvs_flash_init 계열은 여기서만 */
    ESP_ERROR_CHECK(app_events_init());
    ESP_ERROR_CHECK(board_init());          /* LED 부팅 표시가 가장 먼저 켜진다 */
    led_ring_set_brightness(settings_get_led_brightness());
    restore_time_from_rtc();
    ESP_ERROR_CHECK(audio_core_init());     /* I2S → 무음 출력 → 50ms 뒤 앰프 */
    log_heap("audio");

    net_set_time_sync_cb(on_time_sync);
    ESP_ERROR_CHECK(net_init());
    log_heap("net");
    ESP_ERROR_CHECK(wake_init());
    log_heap("wake");
    ESP_ERROR_CHECK(live_voice_init());
    log_heap("live_voice");
    ESP_ERROR_CHECK(assistant_init());
    log_heap("assistant");
    optional_init("radio", media_radio_init());
    optional_init("spotify", media_spotify_init());
    optional_init("telegram", telegram_init());
    optional_init("notes", notes_init());   /* media_radio_init()이 먼저 돌아 /storage가 이미 마운트돼 있다 */
    ESP_ERROR_CHECK(phones_init());
    optional_init("ble", ble_prov_init());
    ble_prov_set_memory_hooks(ble_before_open, ble_after_close);
    ESP_ERROR_CHECK(board_buttons_start());

    /* 드라이버 초기화와 NVS 설정 적용이 끝난 뒤에 이벤트 처리를 시작한다 (2.19절) */
    ESP_ERROR_CHECK(app_events_subscribe(ESP_EVENT_ANY_ID, on_app_event, NULL));
    start_led_timer();
    start_notes_timer();
    if (dev_console_start() != ESP_OK) {
        ESP_LOGW(TAG, "dev console unavailable");
    }
    led_ring_set_booting(false);
    app_events_set_ready();
    audio_core_play_startup();

    /* Wi-Fi 실패만으로는 프로비저닝 모드에 들어가지 않는다. Key2 3초로 BLE 창을 연다 */
    if (!settings_is_provisioned()) {
        s_mode = SYS_MODE_PROVISIONING;
        ESP_LOGI(TAG, "not provisioned: BLE provisioning mode (or use the dev console)");
        ESP_ERROR_CHECK(ble_prov_open_unlimited());
        return;
    }

    start_services();
    log_heap("boot done");
}
