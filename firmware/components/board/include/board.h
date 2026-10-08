/**
 * @file board.h
 * @brief Waveshare ESP32-S3-AUDIO-Board 하드웨어 (설계서 2.0, 2.15, 2.19절)
 *
 * I2C 버스, TCA9555 확장 칩(앰프·버튼), 코덱(ES8311/ES7210), LED 링, 버튼 폴링, RTC.
 * I2C 버스는 코덱·확장 칩·RTC가 공유한다. IDF 5.x i2c_master 드라이버가 버스 단위로
 * 트랜잭션을 직렬화하므로 태스크마다 별도 뮤텍스는 두지 않는다.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include "esp_err.h"
#include "driver/i2s_types.h"
#include "led_ring.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * LED 링(부팅 노란 깜빡임) → I2C → TCA9555(앰프 LOW, 버튼 입력) → RTC 순서로 초기화한다.
 * 코덱은 I2S 채널이 필요하므로 audio_core가 board_codec_open()으로 연다.
 */
esp_err_t board_init(void);

/** 스피커 앰프(EXIO8). 코덱 초기화 후, TX가 무음을 내보내기 시작한 다음 켠다 */
esp_err_t board_amp_enable(bool on);

/* ── 코덱 (codec.c) ──────────────────────────────────────── */

/**
 * ES7210(마이크)·ES8311(스피커)를 연다. I2S 채널은 이미 enable된 상태여야 한다
 * (MCLK가 먼저 나가야 코덱 설정이 된다). ES7210은 실패하면 50ms 뒤 최대 3회 재시도한다.
 */
esp_err_t board_codec_open(i2s_chan_handle_t tx, i2s_chan_handle_t rx, uint32_t sample_rate);
esp_err_t board_codec_read(void *data, size_t bytes);
esp_err_t board_codec_write(const void *data, size_t bytes);
esp_err_t board_codec_set_volume(int volume);       /* 0–80 */
esp_err_t board_codec_set_mic_gain(float db);       /* 0–37.5 */
esp_err_t board_codec_set_mic_mute(bool mute);

/* ── 버튼 (buttons.c) ────────────────────────────────────── */

/**
 * TCA9555를 20ms마다 폴링하는 태스크를 만든다. 태스크는 app_events_wait_ready() 뒤에
 * 동작을 시작하고, 결과는 APP_EVT_BUTTON 이벤트로 보낸다.
 */
esp_err_t board_buttons_start(void);

/* ── RTC (rtc.c, PCF85063) ───────────────────────────────── */

/** 시각이 유효하면(발진기 정지 플래그 없음, 2026년 이후) true */
bool      board_rtc_read(struct tm *out);
esp_err_t board_rtc_write(const struct tm *utc);

/* 내부용: 다른 board 파일이 같은 I2C 버스·확장 칩을 쓴다 */
void *board_i2c_bus(void);
void *board_io_expander(void);

#ifdef __cplusplus
}
#endif
