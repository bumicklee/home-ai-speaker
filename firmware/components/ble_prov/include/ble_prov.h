/**
 * @file ble_prov.h
 * @brief BLE 프로비저닝 GATT 서버 (설계서 2.13절)
 *
 * 서비스: Wi-Fi 0xFF50, 기기 정보 0xFF60, 설정 0xFF80. 쓰기는 모두 링크 암호화
 * (LE Secure Connections, Just Works, 본딩 없음).
 *
 * BLE 창: 첫 부팅은 시간 제한 없이, Key2 3초로 연 창은 연결이 없으면 5분, 연결 중에도
 * 최대 15분. 창을 열 때마다 NimBLE을 새로 초기화하고, 닫을 때 완전히 내린다.
 * 열림/닫힘은 APP_EVT_PROV_START / APP_EVT_PROV_DONE으로 알린다.
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 작업 태스크를 만든다. NimBLE은 창을 열 때 올린다 */
esp_err_t ble_prov_init(void);

/** Key2 3초로 여는 창 (5분 / 최대 15분) */
esp_err_t ble_prov_open_window(void);
/** 미설정 첫 부팅: 시간 제한 없음 */
esp_err_t ble_prov_open_unlimited(void);
/** 창 닫기 (작업 태스크가 처리, 바로 돌아옴) */
void      ble_prov_close_window(void);

/**
 * BLE를 올리기 직전·내린 직후에 부를 함수. BLE 컨트롤러는 내부 RAM만 쓰므로 main이 다른 쪽 메모리를 잠시 비운다.
 * ble_worker 태스크(PSRAM 스택)에서 불린다.
 */
typedef void (*ble_prov_memory_hook_t)(void);
void ble_prov_set_memory_hooks(ble_prov_memory_hook_t before_open, ble_prov_memory_hook_t after_close);
bool      ble_prov_is_open(void);

#ifdef __cplusplus
}
#endif
