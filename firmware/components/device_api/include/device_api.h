/**
 * @file device_api.h
 * @brief BLE(ble_prov)와 LAN(local_api)이 함께 쓰는 기기 제어 계층 (설계서 2.13, 2.14, 4절)
 *
 * 같은 설정을 두 경로가 따로 해석하면 스키마가 어긋난다. 그래서 JSON 변환·검증·적용을
 * 이 모듈 한 곳에서 한다. 설정 그룹 스키마는 LAN /api/v1/settings/{group}와 같다.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "cJSON.h"
#include "esp_err.h"
#include "secret_store.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── 설정 그룹 (device_settings.c) ───────────────────────── */

/** group: audio · wake · assistant · led. 모르는 그룹이면 NULL */
cJSON    *device_settings_get(const char *group);

/**
 * 주어진 키만 바꾼다(부분 수정). 모르는 키·타입·범위가 틀리면 ESP_ERR_INVALID_ARG,
 * 모르는 그룹이면 ESP_ERR_NOT_FOUND. 성공하면 즉시 적용하고 저장한 뒤
 * APP_EVT_SETTINGS_CHANGED를 보낸다.
 */
esp_err_t device_settings_set(const char *group, const cJSON *value);

/* ── 기기 정보 (device_info.c) ───────────────────────────── */

/** BLE device.info: protocol_version, fw, mac, provisioned, wifi_connected, secrets */
cJSON *device_info(void);
/** LAN /status */
cJSON *device_status(void);
/** 키별 저장 여부 {"openai_key": true, ...}. 값은 절대 넣지 않는다 */
cJSON *device_secrets_state(void);
/** STA MAC 12자 대문자 16진수 */
void   device_mac_string(char out[13]);

/**
 * 비밀값 이름 → ID. 앱이 쓸 수 있는 이름만 받는다
 * (openai_key, tg_bot_token, stock_key). Spotify 값은 spotify.link 전용이라 거부한다.
 */
bool device_secret_from_name(const char *name, secret_id_t *out);

/* ── 휴대폰 토큰 (phones.c) ──────────────────────────────── */

#define PHONE_ID_MAX    36      /* UUID 문자열 */
#define PHONE_NAME_MAX  32
#define PHONE_TOKEN_LEN 43      /* 32바이트 base64url (패딩 없음) */

esp_err_t phones_init(void);

/**
 * 토큰을 새로 만든다. 같은 phone_id면 기존 토큰을 교체한다.
 * 5대가 차 있으면 ESP_ERR_INVALID_SIZE (evict에 지울 phone_id를 주면 그걸 지우고 등록).
 * 기기는 토큰의 SHA-256만 저장한다.
 */
esp_err_t phones_pair(const char *phone_id, const char *name, const char *evict, char token[PHONE_TOKEN_LEN + 1]);

/** Bearer 토큰 확인 (상수 시간 비교). 맞으면 phone_id를 채운다 */
bool phones_verify(const char *token, char phone_id[PHONE_ID_MAX + 1]);

/** [{phone_id, name, created, self}] */
cJSON    *phones_list(const char *self_phone_id);
esp_err_t phones_revoke(const char *phone_id);
esp_err_t phones_clear(void);

#ifdef __cplusplus
}
#endif
