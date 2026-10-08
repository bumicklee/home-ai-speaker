/* Based on waveshare-ai-speaker-assistant (MIT, Copyright (c) 2026 KMX415). See THIRD_PARTY_NOTICES.md. */
/**
 * @file mic_control.h
 * @brief 마이크 자동 게인 + 음성 안내 보정 (기존 펌웨어 mic_control.c)
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "cJSON.h"
#include "esp_err.h"

/** 저장된 게인을 읽고 audio_core의 마이크 처리 훅으로 등록한다 */
void mic_control_init(void);
void mic_control_process(int16_t *clean, const int16_t *raw, size_t count);
void mic_control_tick(void);
void mic_control_cancel(void);
bool mic_control_active(void);
esp_err_t mic_control_start(void);
cJSON *mic_control_status(void);
unsigned mic_control_selftest(void);    /* 통과하면 0xFF */

int       mic_gain(void);
bool      mic_auto_gain(void);
esp_err_t mic_set(int gain, bool automatic, bool persist);
