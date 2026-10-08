/**
 * @file led_ring.h
 * @brief WS2812B × 7 LED 링 표시 (설계서 2.15절)
 *
 * 표시는 상태 플래그로 정하고, 우선순위가 높은 표시가 낮은 표시를 가린다.
 * 상태는 main의 상태머신이 이벤트를 받아 설정한다(LED 모듈은 다른 모듈을 모른다).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "app_events.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LED_KEY2_NONE,
    LED_KEY2_BLE,       /* 3초 도달: 하늘색 링 */
    LED_KEY2_WARN,      /* 7초 도달: 빨간색 링이 차오름 */
    LED_KEY2_FACTORY,   /* 10초 도달: 빨간색 빠른 깜빡임 */
} led_key2_stage_t;

/** RMT 장치를 만들고 표시 태스크를 시작한다. 시작하자마자 부팅 표시(노란 깜빡임) */
esp_err_t led_ring_init(void);

void led_ring_set_brightness(uint8_t percent);      /* 0–100. 0이면 우선순위 1–3만 표시 */
void led_ring_set_booting(bool on);
void led_ring_set_key2_stage(led_key2_stage_t stage);
void led_ring_show_error(void);                      /* 빨간색 3초 깜빡임 */
void led_ring_set_ble_window(bool on);
void led_ring_set_wifi_connecting(bool on);
void led_ring_flash_wake(void);                      /* 초록색 번쩍임 */
void led_ring_set_voice(voice_state_t state);
void led_ring_set_wake_armed(bool on);
void led_ring_set_unread(bool on);
/** 일정 알림을 대화로 전할 수 없었음(인터넷·API 키 없음): 주황 표시. 다음 대화가 시작되면 꺼진다 */
void led_ring_set_reminder(bool on);
/** 음악: 버퍼링 = 흰색 느린 깜빡임, 재생 = 흰색 은은한 레벨 미터 (RAD-07) */
void led_ring_set_media(media_play_state_t state);

/** 재생 중인 소리의 최대 진폭(0–32767). 오디오 태스크가 20ms마다 넣는다 */
void led_ring_feed_level(int16_t peak);

#ifdef __cplusplus
}
#endif
