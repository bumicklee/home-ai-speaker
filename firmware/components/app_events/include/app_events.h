/**
 * @file app_events.h
 * @brief esp_event 기반 앱 이벤트 버스 (설계서 2.3절)
 *
 * 하위 모듈은 상위 모듈 헤더를 include하지 않고 이 버스로 알린다.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

ESP_EVENT_DECLARE_BASE(APP_EVENT);

typedef enum {
    APP_EVT_WIFI_UP,
    APP_EVT_WIFI_DOWN,
    APP_EVT_PROV_START,
    APP_EVT_PROV_DONE,
    APP_EVT_WAKE_DETECTED,
    APP_EVT_VOICE_STATE,        /* data: voice_state_t */
    APP_EVT_MEDIA_STATE,        /* data: media_state_t + now playing */
    APP_EVT_TG_MESSAGE,         /* data: int 읽지 않은 수 (0이면 다 읽음) */
    APP_EVT_TG_PAIRED,          /* data: int64_t chat_id */
    APP_EVT_OTA,                /* data: 단계 + 진행률 */
    APP_EVT_WAKE_TEST,          /* data: 감지 횟수 */
    APP_EVT_BUTTON,             /* data: app_button_event_t */
    APP_EVT_SETTINGS_CHANGED,   /* data: 설정 키 ID */
    APP_EVT_ERROR,              /* data: 오류 코드 */
    APP_EVT_TG_STATUS,          /* data: tg_state_t (int) */
    APP_EVT_WAKE_READY,         /* 호출어 모델이 올라가 듣기 시작함 (모델을 다시 열 때마다) */
    APP_EVT_LESSON,             /* 영어 수업 상태·정리가 바뀜 (내용은 lesson_json으로 읽는다) */
    APP_EVT_NOTES,              /* 메모·일정이 바뀜(말·앱·알림). data: uint32_t 현재 rev */
    APP_EVT_NOTES_TICK,         /* 30초마다: 알릴 것·놓친 것을 검사하라 (notes_tick). 데이터 없음 */
    APP_EVT_REMINDER,           /* 일정 알림 결과. data: app_notes_reminder_t */
} app_event_id_t;

/* ── 이벤트 데이터 타입 (여러 모듈이 공유) ───────────────── */

typedef enum {
    VOICE_IDLE,
    VOICE_WAKE_ACK,
    VOICE_CONNECTING,
    VOICE_LIVE,
    VOICE_CLOSING,
} voice_state_t;

typedef enum {
    APP_BUTTON_KEY1,    /* 볼륨 내림 */
    APP_BUTTON_KEY2,    /* 시스템 버튼 */
    APP_BUTTON_KEY3,    /* 볼륨 올림 */
} app_button_t;

typedef enum {
    APP_PRESS_SHORT,        /* 짧게 눌렀다 뗌 (Key2는 3초 미만) */
    APP_PRESS_REPEAT,       /* Key1·Key3 길게 누르는 동안 반복 */
    APP_PRESS_HOLD_3S,      /* Key2 3초 도달 (아직 누르는 중) */
    APP_PRESS_HOLD_7S,      /* Key2 7초 도달 (경고) */
    APP_PRESS_BLE_WINDOW,   /* Key2 3–7초 사이에 뗌 */
    APP_PRESS_CANCEL,       /* Key2 7–10초 사이에 뗌 */
    APP_PRESS_FACTORY,      /* Key2 10초 도달: 즉시 공장 초기화 */
} app_press_t;

typedef struct {
    app_button_t button;
    app_press_t press;
} app_button_event_t;

/* APP_EVT_MEDIA_STATE 데이터 */
typedef enum {
    MEDIA_SOURCE_NONE,
    MEDIA_SOURCE_RADIO,
    MEDIA_SOURCE_SPOTIFY,
} media_source_t;

typedef enum {
    MEDIA_STOPPED,
    MEDIA_BUFFERING,
    MEDIA_PLAYING,
    MEDIA_PAUSED,
} media_play_state_t;

typedef struct {
    uint8_t source;     /* media_source_t */
    uint8_t state;      /* media_play_state_t */
} app_media_event_t;

/* APP_EVT_REMINDER 데이터. status: 0 sent, 1 acked, 2 no_answer, 3 missed (notes.c의 alert_status_t와 같은 순서) */
typedef struct {
    char id[11];        /* "n_" + 8자리 16진 + NUL */
    uint8_t status;
} app_notes_reminder_t;

/* APP_EVT_SETTINGS_CHANGED 데이터 */
typedef enum {
    APP_SETTING_VOLUME,
    APP_SETTING_AUDIO,
    APP_SETTING_WAKE,
    APP_SETTING_ASSISTANT,
    APP_SETTING_LED,
    APP_SETTING_SECRETS,
    APP_SETTING_PHONES,
    APP_SETTING_TELEGRAM,
} app_setting_t;

/* APP_EVT_ERROR 데이터. 고정 코드만 보낸다 (외부 응답 본문은 넣지 않음) */
typedef enum {
    APP_ERR_NONE,
    APP_ERR_NO_API_KEY,
    APP_ERR_WIFI,
    APP_ERR_CLOCK,
    APP_ERR_VOICE_CONNECT,
    APP_ERR_VOICE_REJECTED,
    APP_ERR_VOICE_DROPPED,
    APP_ERR_VOICE_INTERNAL,
    APP_ERR_RADIO_UNAVAILABLE,  /* 5회 재연결 실패 또는 지원하지 않는 형식 */
    APP_ERR_AI_UNAVAILABLE,     /* 고른 AI(provider)가 이 펌웨어 단계에서 아직 없음 (docs/design-voice-saver-mode.md) */
} app_error_t;

/** 전용 이벤트 루프를 만든다. 다른 모듈보다 먼저 한 번 부른다. */
esp_err_t app_events_init(void);

esp_err_t app_events_post(app_event_id_t id, const void *data, size_t len, uint32_t timeout_ms);
esp_err_t app_events_subscribe(app_event_id_t id, esp_event_handler_t handler, void *arg);

/**
 * 드라이버 초기화와 NVS 설정 적용이 끝났음을 알린다 (2.19절 "초기화 완료" 비트).
 * 이 비트가 서기 전에는 이벤트를 처리하는 태스크가 동작을 시작하지 않는다.
 */
void app_events_set_ready(void);
bool app_events_wait_ready(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
