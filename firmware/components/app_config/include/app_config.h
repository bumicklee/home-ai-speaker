/**
 * @file app_config.h
 * @brief 버전, 타이밍, 식별자 상수 (led-app 방식 중앙 설정)
 *
 * BLE·LAN 규격 상수는 설계서 2.13·2.14절이 기준이다. 바꿀 때는 앱의
 * ble_protocol.dart·api_paths.dart와 같은 PR에서 함께 바꾼다.
 */
#pragma once

/* ── 버전 ──────────────────────────────────────────────────── */
#define APP_FW_VERSION              "0.1.0"
#define APP_PRODUCT_CODE            "AISPK-S3-V1"
#define APP_PROTOCOL_VERSION        1

/* ── 파티션 ───────────────────────────────────────────────── */
#define APP_PART_SECRETS            "secrets"
#define APP_PART_STORAGE            "storage"
#define APP_PART_MODEL              "model"

/* ── BLE (2.13절) ─────────────────────────────────────────── */
#define APP_BLE_NAME_PREFIX         "JARVIS_"
#define APP_BLE_SVC_WIFI            0xFF50
#define APP_BLE_CHR_SSID            0xFF51
#define APP_BLE_CHR_PASSWORD        0xFF52
#define APP_BLE_CHR_STATUS          0xFF53
#define APP_BLE_CHR_SCAN            0xFF54
#define APP_BLE_SVC_DEVICE          0xFF60
#define APP_BLE_CHR_MAC             0xFF61
#define APP_BLE_CHR_PRODUCT         0xFF62
#define APP_BLE_CHR_FIRMWARE        0xFF63
#define APP_BLE_SVC_CONFIG          0xFF80
#define APP_BLE_CHR_CMD_RX          0xFF81
#define APP_BLE_CHR_CMD_TX          0xFF82
#define APP_BLE_CHR_PROV_DONE       0xFF88

#define APP_BLE_WINDOW_MS           (5 * 60 * 1000)    /* 연결 없으면 5분 뒤 닫힘 */
#define APP_BLE_WINDOW_MAX_MS       (15 * 60 * 1000)   /* 연결 중 연장 최대 15분 */
#define APP_BLE_CMD_MAX_BYTES       4096
#define APP_BLE_CMD_REASSEMBLY_MS   3000
#define APP_PROV_REBOOT_DELAY_MS    2000

/* ── LAN (2.14절) ─────────────────────────────────────────── */
#define APP_MDNS_SERVICE            "_aispeaker"
#define APP_MDNS_PROTO              "_tcp"
#define APP_MDNS_HOST_PREFIX        "jarvis-"
#define APP_HTTP_PORT               80
#define APP_API_MAX_PHONES          5

/* ── Wi-Fi ────────────────────────────────────────────────── */
#define APP_WIFI_CONNECT_TIMEOUT_MS 20000   /* 연결 판정 20초 (상태 5 = 시간 초과) */
#define APP_WIFI_RETRY_MIN_MS       1000    /* 재연결 지수 백오프 시작 */
#define APP_WIFI_RETRY_MAX_MS       60000   /* 백오프 상한. 재시도는 멈추지 않는다 */
#define APP_WIFI_DHCP_GRACE_MS      30000   /* 연결됐지만 IP를 못 받으면 끊고 다시. 15초로는 공유기가 늦을 때 계속 끊었다 (실기기) */
#define APP_WIFI_DHCP_CHECK_MS      2000
#define APP_SNTP_SERVER             "pool.ntp.org"
#define APP_TIME_VALID_EPOCH        1767225600  /* 2026-01-01 UTC. 이보다 이르면 시각 미설정 */

/* ── 음성 대화 ────────────────────────────────────────────── */
/* 무응답은 요금 때문에 줄였다 (2026-10-04 사용자). 영어 수업은 자기 제한을 따로 쓴다 */
#define APP_VOICE_IDLE_TIMEOUT_MS   (20 * 1000)        /* 무응답 20초 (60 → 30 → 15 → 30 → 20초, 책 16장 버그 2) */
#define APP_VOICE_MAX_SESSION_MS    (10 * 60 * 1000)   /* 대화 최대 10분 */
#define APP_VOICE_USAGE_WAIT_MS     (15 * 1000)        /* 종료 후 사용량 대기 */
#define APP_LIVE_MODEL_DEFAULT      "gpt-live-1"
#define APP_BACKEND_MODEL_DEFAULT   "gpt-5.6-luna"
/* Gemini 절약 모드 (docs/design-voice-saver-mode.md stage 2, 2026-10 ai.google.dev 확인: 현재 값 그대로) */
#define APP_GEMINI_MODEL_DEFAULT     "gemini-3.1-flash-lite"
#define APP_GEMINI_TTS_MODEL_DEFAULT "gemini-3.8-flash-lite-tts"
/* Gemini 실시간(Live API) 3단계. 모델 이름은 "models/" 없이 짧은 형태로 두고, gemini_live.c가
 * BidiGenerateContentSetup.model에 넣을 때 "models/"를 붙인다 (2026-10 ai.google.dev/api/live,
 * /gemini-api/docs/live-api/tools 확인: gemini-3.8-live, 저지연 음성 대화의 기본값) */
#define APP_GEMINI_LIVE_MODEL_DEFAULT "gemini-3.8-live"

/* ── Spotify (2.10절) ─────────────────────────────────────── */
/* Connect 수신기 이름 접두어. 같은 계정의 기기 중 이 이름이 있으면 그 기기로 재생한다 */
#define APP_SPOTIFY_DEVICE_NAME     "Jarvis"

/* ── 버튼 (2.15절) ────────────────────────────────────────── */
#define APP_BUTTON_POLL_MS          20
#define APP_BUTTON_DEBOUNCE_COUNT   2       /* 연속 2회(40ms) 같은 값이면 확정 */
#define APP_KEY2_BLE_MS             3000
#define APP_KEY2_WARN_MS            7000
#define APP_KEY2_FACTORY_MS         10000
#define APP_VOLUME_REPEAT_MS        300

/* ── 오디오 ───────────────────────────────────────────────── */
#define APP_VOLUME_MAX              80
#define APP_VOLUME_STEP             5
#define APP_MIC_GAIN_MAX_DB         37.5f
#define APP_AMP_ON_DELAY_MS         50      /* TX 무음 출력 후 앰프 켜기까지 */
