/* Based on waveshare-ai-speaker-assistant (MIT, Copyright (c) 2026 KMX415). See THIRD_PARTY_NOTICES.md. */
/**
 * @file wake.h
 * @brief WakeNet9 호출어 감지 (설계서 2.7절, 기존 펌웨어 wake_word.c 이식)
 *
 * 감지하면 APP_EVT_WAKE_DETECTED를 보낸다. 대화를 시작하는 것은 main의 몫이다.
 * 대화 중에는 TLS 메모리를 확보하려고 모델을 내린다(wake_pause / wake_resume).
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "settings.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 저장된 설정을 읽고 마이크 싱크를 등록한다. 모델은 wake_start() 뒤에 올린다 */
esp_err_t wake_init(void);
esp_err_t wake_start(void);
esp_err_t wake_stop(void);

/** 모델을 내리고 메모리를 돌려받을 때까지 최대 5초 기다린다. 성공하면 true */
bool wake_pause(void);
void wake_resume(void);

/** 설정 변경 (저장 + 즉시 반영) */
esp_err_t wake_set_config(const settings_wake_t *cfg);
void      wake_get_config(settings_wake_t *out);

/** 감지 시험 모드 60초: 감지해도 대화를 시작하지 않고 APP_EVT_WAKE_TEST만 보낸다 */
esp_err_t wake_test(bool enable);
bool      wake_is_testing(void);

/** 지금 호출어를 듣고 있으면 true (LED 대기 표시) */
bool wake_is_listening(void);

#ifdef __cplusplus
}
#endif
