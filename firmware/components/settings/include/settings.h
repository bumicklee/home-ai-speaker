/**
 * @file settings.h
 * @brief 일반 설정 저장소 (설계서 2.12절, led-app nvs_storage 방식)
 *
 * 항목별로 타입이 정해진 get/set 함수를 둔다. 저장값이 없거나 범위를 벗어나면
 * 기본값을 돌려준다. 비밀값은 secret_store.h를 쓴다.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int  volume;            /* 0–80, 기본 40 */
    int  mic_gain;          /* 0–36 dB, 기본 36 (실기기에서 30 이하는 호출어가 안 들림) */
    bool auto_gain;         /* 기본 false */
    int  duck_db;           /* 0–40, 기본 20 */
    bool sp_pause_voice;    /* 기본 false */
} settings_audio_t;

typedef struct {
    bool    enabled;        /* 기본 true */
    uint8_t phrase;         /* 0 Alexa (2026-10-04, 전에는 Jarvis), 1 Computer */
    uint8_t sensitivity;    /* 0–100, 기본 50 */
    uint8_t gain_db;        /* 호출어 입력 디지털 게인 0–24 dB, 기본 12 (실기기에서 입력이 작았음) */
} settings_wake_t;

/**
 * nvs·secrets 파티션을 초기화한다. nvs_flash_init() 계열 호출은 이 함수에서만 한다.
 */
esp_err_t settings_init(void);

/* 프로비저닝 */
bool      settings_is_provisioned(void);
esp_err_t settings_set_provisioned(bool provisioned);

/* Wi-Fi (led-app과 같은 키) */
bool      settings_has_wifi(void);
esp_err_t settings_get_wifi(char ssid[33], char password[65]);
esp_err_t settings_set_wifi(const char *ssid, const char *password);

/* 오디오 */
void      settings_get_audio(settings_audio_t *out);
esp_err_t settings_set_audio(const settings_audio_t *in);

/* 호출어 */
void      settings_get_wake(settings_wake_t *out);
esp_err_t settings_set_wake(const settings_wake_t *in);

/* LED 밝기 0–100, 기본 60 */
uint8_t   settings_get_led_brightness(void);
esp_err_t settings_set_led_brightness(uint8_t brightness);

/* 문자열 값 (비서 설정 JSON 등). out_len에는 버퍼 크기를 넣고, 성공하면 실제 길이(널 포함)가 들어간다 */
esp_err_t settings_get_str(const char *ns, const char *key, char *out, size_t *out_len);
esp_err_t settings_set_str(const char *ns, const char *key, const char *value);

/** 공장 초기화: nvs·secrets 파티션을 지운다. 재부팅은 호출한 쪽이 한다. */
esp_err_t settings_factory_reset(void);

#ifdef __cplusplus
}
#endif
