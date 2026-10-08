/**
 * @file saver_voice.h
 * @brief live_voice.c ↔ saver_voice.c 전용 내부 계약 (docs/design-voice-saver-mode.md 11.5·11.6)
 *
 * components/live_voice 안에서만 보인다 (CMakeLists의 PRIV_INCLUDE_DIRS "."). 다른 컴포넌트는 이 파일을
 * 모른다. live_voice.c가 세션 공유 상태(바쁨·상태·준비·문구·세대·정지 요청)를 들고 있고, saver_voice.c는
 * 그 상태를 이 작은 전달 함수들로만 건드린다. 실시간(openai.live) 경로의 기존 static 변수는 그대로 둔다.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "app_events.h"
#include "cJSON.h"
#include "esp_err.h"
#include "live_voice.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── live_voice.c가 구현, saver_voice.c가 부른다 ─────────────── */

void     voice_shared_set_state(voice_state_t state);
void     voice_shared_set_ready(bool ready);
/** 고정 문자열만 (내용·키는 절대 안 됨) */
void     voice_shared_set_status(const char *text);
bool     voice_shared_stopping(void);
/** 세션 마무리 공통부: ready=false, (실패 아니면 status="Ready"), busy=false, VOICE_IDLE */
void     voice_shared_end(bool had_error);
/** 실시간 엔진이 맡아 둔 내부 RAM 자리(약 12KB)를 대화 동안 풀고, 끝나면 다시 맡는다 */
void     voice_shared_reserve_release(void);
void     voice_shared_reserve_take(void);
/** 등록된 핸들러(assistant_init이 live_voice_set_handlers로 넣어 둔 것). 세션 시작 전에 고정됨 */
const live_voice_handlers_t *voice_shared_handlers(void);
/** live_voice_set_next_limits()로 들어온 값(0이면 기본값 적용은 호출한 쪽 몫)을 한 번 꺼낸다 */
void     voice_shared_take_next_limits(uint32_t *max_ms, uint32_t *idle_ms);

/* ── saver_voice.c가 구현, live_voice.c가 부른다 ─────────────── */

/** 부팅 때 한 번: 세션 태스크를 만든다 (스택 PSRAM) */
esp_err_t saver_voice_init(void);
/** live_voice_start()가 세션 태스크를 깨울 때 쓰는 태스크 진입점 (ulTaskNotifyTake 루프) */
void      saver_voice_task_entry(void *arg);
/** Key2 짧게: 지금 재생만 멈추고 듣기로. 절약 세션이 없으면 아무 일도 안 함 */
void      saver_voice_barge_in(void);
/**
 * live_voice_text()가 saver 세션이 active일 때 넘기는 글(지금은 알림 문장만, 11.7절). AI 없이 그대로
 * 합성만 하고 다시 듣기로 돌아간다. live_voice_ready()가 true가 아니면 ESP_ERR_INVALID_STATE
 */
esp_err_t saver_voice_text(const char *text);

/**
 * 비서 설정의 목소리 이름을 Gemini TTS/Live의 prebuilt 목소리 이름으로 바꾼다(겹치는 이름 있으면 그대로,
 * 없으면 "Kore"). saver_voice.c(절약 모드 합성)와 gemini_live.c(3단계, speechConfig) 둘 다 쓴다.
 */
const char *gemini_tts_voice(const char *voice);

#ifdef __cplusplus
}
#endif
