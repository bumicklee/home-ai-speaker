/**
 * @file net.h
 * @brief Wi-Fi 스테이션 관리(led-app wifi_manager 이식)와 시간 동기화 (설계서 2.4절)
 *
 * - 재연결은 지수 백오프로 멈추지 않고 계속한다. Wi-Fi 실패만으로 프로비저닝에 들어가지 않는다.
 * - "연결됐지만 IP를 못 받는" 상태는 DHCP 워치독이 끊고 다시 시도한다.
 * - 연결/끊김은 APP_EVT_WIFI_UP / APP_EVT_WIFI_DOWN 이벤트로 알린다.
 * - 프로비저닝 중에는 저장하지 않은 새 Wi-Fi 정보로 시험 연결을 할 수 있다 (net_test_credentials).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_wifi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** BLE 0xFF53 상태 코드와 같은 값 (2.13절) */
typedef enum {
    NET_WIFI_IDLE = 0,
    NET_WIFI_CONNECTING = 1,
    NET_WIFI_SUCCESS = 2,
    NET_WIFI_NO_SSID = 3,
    NET_WIFI_AUTH_FAIL = 4,
    NET_WIFI_TIMEOUT = 5,
} net_wifi_status_t;

/** netif, 기본 이벤트 루프, Wi-Fi 드라이버를 준비한다 (연결은 아직 안 함) */
esp_err_t net_init(void);

/** 저장된 Wi-Fi로 연결을 시작하고 SNTP를 켠다. 저장된 Wi-Fi가 없으면 ESP_ERR_NOT_FOUND */
esp_err_t net_start(void);

bool net_is_connected(void);
bool net_wait_connected(uint32_t timeout_ms);

/** Wi-Fi STA MAC (기기 식별자, 2.13절) */
esp_err_t net_get_sta_mac(uint8_t mac[6]);

/** 대기 중에는 절전(true), 음성·음악 재생 중에는 끈다(false) (2.19절) */
void net_set_power_save(bool enable);

/** 끊김 사유 → 0xFF53 상태 코드 */
net_wifi_status_t net_status_from_reason(uint8_t reason);
uint8_t           net_last_disconnect_reason(void);

/* ── 프로비저닝용 시험 연결 (2.13절 0xFF53 규칙) ────────── */

/** 시험 결과. 시도 한 번에 종료 상태(2–5)는 정확히 한 번만 불린다 (이벤트 태스크에서, 막지 않는다) */
typedef void (*net_test_cb_t)(net_wifi_status_t status);

/**
 * 저장하지 않고 새 정보로 연결을 시도한다. 20초 안에 IP를 못 받으면 NET_WIFI_TIMEOUT.
 * 시험하는 동안은 자동 재연결(백오프)을 하지 않는다. 다시 부르면 이전 시도는 버린다.
 */
esp_err_t net_test_credentials(const char *ssid, const char *password, net_test_cb_t cb);

/** 시험을 끝낸다. keep=true면 지금 연결을 유지하고, false면 저장된 Wi-Fi로 되돌린다 */
void net_end_test(bool keep);
bool net_is_testing(void);

/**
 * 주변 AP 스캔 (막는 함수, 약 2–3초). 필요하면 Wi-Fi를 켠다.
 * 돌려받은 개수, 실패하면 -1. GATT 콜백 안에서 부르지 않는다.
 */
int net_scan(wifi_ap_record_t *out, uint16_t max);

/* ── mDNS (net_mdns.c, 2.14절) ──────────────────────────── */

/** jarvis-XXXXXX.local, _aispeaker._tcp:80, TXT id·pv */
esp_err_t net_mdns_start(void);

/* ── 시간 (net_time.c) ───────────────────────────────────── */

/** 시스템 시각이 유효한가 (RTC 복원 또는 SNTP 동기화 후) */
bool net_time_valid(void);

/** SNTP 동기화가 끝날 때마다 불린다 (예: RTC 갱신). net_start() 전에 등록한다 */
typedef void (*net_time_sync_cb_t)(void);
void net_set_time_sync_cb(net_time_sync_cb_t cb);

/* 내부용 */
esp_err_t net_time_start(void);

#ifdef __cplusplus
}
#endif
