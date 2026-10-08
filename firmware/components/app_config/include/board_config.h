/**
 * @file board_config.h
 * @brief Waveshare ESP32-S3-AUDIO-Board 핀·I2C 주소 (설계서 2.0절)
 *
 * 펌웨어의 모든 핀 번호는 이 파일에서만 가져온다. 검증된 값만 적는다.
 * 쓰면 안 되는 GPIO: 26–32(플래시), 33–37(옥탈 PSRAM), 0·3·45·46(스트래핑),
 * 19·20(USB), 43·44(UART0 콘솔).
 */
#pragma once

#include "driver/gpio.h"
#include "driver/i2c_types.h"
#include "driver/i2s_types.h"

/* ── LED 링 (WS2812B × 7, RMT) ────────────────────────────── */
#define BOARD_LED_GPIO              GPIO_NUM_38
#define BOARD_LED_COUNT             7
#define BOARD_LED_RMT_RES_HZ        (10 * 1000 * 1000)
/* 전류 제한: 채널 값 상한 (USB 전원 + 앰프 동시 사용) */
#define BOARD_LED_CHANNEL_MAX       128

/* ── I2C (코덱·확장 칩·RTC 공유, 뮤텍스로 보호) ───────────── */
#define BOARD_I2C_PORT              I2C_NUM_0
#define BOARD_I2C_SDA_GPIO          GPIO_NUM_11
#define BOARD_I2C_SCL_GPIO          GPIO_NUM_10
#define BOARD_I2C_FREQ_HZ           100000

#define BOARD_I2C_ADDR_ES8311       0x18
#define BOARD_I2C_ADDR_ES7210       0x40
#define BOARD_I2C_ADDR_TCA9555      0x20
#define BOARD_I2C_ADDR_PCF85063     0x51

/* ── I2S (포트 1, 전이중) ──────────────────────────────────── */
#define BOARD_I2S_PORT              I2S_NUM_1
#define BOARD_I2S_MCLK_GPIO         GPIO_NUM_12
#define BOARD_I2S_BCLK_GPIO         GPIO_NUM_13
#define BOARD_I2S_LRCK_GPIO         GPIO_NUM_14
#define BOARD_I2S_DIN_GPIO          GPIO_NUM_15   /* ES7210 → ESP */
#define BOARD_I2S_DOUT_GPIO         GPIO_NUM_16   /* ESP → ES8311 */

/* ── TCA9555 EXIO (인터럽트 미연결, 폴링만) ───────────────── */
#define BOARD_EXIO_PA_EN            8    /* 앰프 Enable, 액티브 하이. 코덱 초기화 후 켠다 */
#define BOARD_EXIO_KEY1             9    /* 볼륨 내림, 액티브 로우 */
#define BOARD_EXIO_KEY2             10   /* 시스템 버튼, 액티브 로우 */
#define BOARD_EXIO_KEY3             11   /* 볼륨 올림, 액티브 로우 */
/* EXIO 6·7(카메라 클럭·USB 먹스)과 0–5, 12–15는 건드리지 않는다 */
