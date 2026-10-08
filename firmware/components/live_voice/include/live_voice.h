/* Based on waveshare-ai-speaker-assistant (MIT, Copyright (c) 2026 KMX415). See THIRD_PARTY_NOTICES.md. */
/**
 * @file live_voice.h
 * @brief OpenAI gpt-live-1 음성 세션 (설계서 2.8절, 기존 펌웨어 live_voice.c 이식)
 *
 * 흐름: Wi-Fi 확인 → 시각 확인(RTC 우선) → WSS 연결 → session.start → 마이크 업로드 루프
 *       → session.close. 상태는 APP_EVT_VOICE_STATE(voice_state_t)로 알린다.
 *
 * 툴 실행은 이 모듈이 하지 않는다. 위임(delegation) 응답에서 함수 호출이 완성되면
 * on_tool_calls 핸들러(assistant의 툴 워커)에 넘기고, 결과는
 * live_voice_submit_tool_outputs()로 받는다. 그동안 업로드 루프는 멈추지 않는다.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "app_events.h"
#include "cJSON.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /** session.start JSON 문자열을 만든다 (호출한 쪽이 free) */
    char *(*build_start_event)(void);
    /** 완성된 function_call 항목 배열. 소유권을 넘긴다. 응답 수신 태스크에서 불리므로 막지 않는다 */
    void (*on_tool_calls)(cJSON *calls, unsigned generation);
    /** 세션 루프에서 약 20ms마다 (마이크 보정 진행용) */
    void (*on_tick)(void);
    /** 세션이 끝날 때 */
    void (*on_session_end)(void);
    /** 사용자 입력이 왔을 때: 입력 전사 조각 또는 live_voice_text (TLG-14, 전송 확인용) */
    void (*on_user_input)(void);
    /** 전사 조각 (from_user: 사용자 말, 아니면 비서 말). 응답 수신 태스크에서 불리므로 막지 않는다 */
    void (*on_transcript)(bool from_user, const char *delta);

    /*
     * AI 고르기 · 실시간/절약 모드 (docs/design-voice-saver-mode.md 11.5). live_voice는 설정을 직접 읽지 않고
     * 이 핸들러들로만 assistant 쪽과 이야기한다 (assistant가 live_voice를 REQUIRES하므로 거꾸로는 안 됨).
     */
    /** true면 설정과 상관없이 openai.live로 (영어 수업, 텔레그램 자동 읽기 1단계 TODO) */
    bool (*force_openai_live)(void);
    /** 절약 모드 세션이 막 시작될 때(듣기 전). 실시간의 assistant_start_event가 하는 guard_reset() 등과 같다 */
    void (*on_session_start)(void);
    /**
     * 절약 모드 지시문(짧게 말하기 포함). 호출한 쪽이 free. voice_out에 저장된 목소리 이름(비서 설정)을
     * 채운다. gemini가 true면 웹 검색 문구를 "없음"으로 바꾼다(Gemini 절약 모드는 검색 도구를 안 준다,
     * docs/design-voice-saver-mode.md stage 2 결정 5) — 그 밖 지시문은 회사와 상관없이 같다
     */
    char *(*build_saver_instructions)(bool gemini, char *voice_out, size_t voice_cap);
    /** 절약 모드 도구 배열 (Responses 형식, delegation.responses.tools와 같은 스키마). 호출한 쪽이 cJSON_Delete */
    cJSON *(*build_saver_tools)(void);
    /** 도구 하나 실행 (assistant_execute 그대로, tool_guard 포함). 호출한 쪽이 cJSON_Delete */
    cJSON *(*execute_tool)(const char *name, const char *arguments);
    /**
     * Gemini Live(3단계) 지시문: 실시간 대화 규칙 + 도구 사용 규칙을 하나로 합치고 "백엔드에 위임"을
     * "직접 호출"로 바꿔 쓴다(Gemini Live는 백엔드 위임이 없다, docs/design-voice-saver-mode.md 2.5·16.4절).
     * voice_out에 비서 설정의 목소리 이름을 채운다(Gemini 목소리로 바꾸는 것은 gemini_live.c가 함,
     * saver_voice.h의 gemini_tts_voice 재사용). search_out에 구글 검색(googleSearch) 도구를 함수 선언과
     * 같이 줄지(설정의 web_search를 그대로 따름 — Gemini Live는 절약 모드의 generateContent REST와 달리
     * 함수 선언과 검색을 같이 쓸 수 있다고 문서에 있다). 호출한 쪽이 free
     */
    char *(*build_gemini_live_instructions)(char *voice_out, size_t voice_cap, bool *search_out);
} live_voice_handlers_t;

esp_err_t live_voice_init(void);
void      live_voice_set_handlers(const live_voice_handlers_t *handlers);

/**
 * 대화 시작. 이미 진행 중이면 ESP_ERR_INVALID_STATE. 설정(ai_provider·mode_openai·mode_gemini)에 따라
 * openai.live(gpt-live-1)·openai.saver·gemini.saver·gemini.live(3단계, Gemini Live API) 네 가지로 나뉜다
 * (docs/design-voice-saver-mode.md 11.5·16.4절). 고른 회사의 키가 없으면 ESP_ERR_INVALID_STATE(+APP_ERR_NO_API_KEY).
 */
esp_err_t live_voice_start(void);
void      live_voice_stop(void);
/**
 * Key2 짧게 누름. 절약 모드이고 지금 비서가 말하는 중이면 재생만 멈추고 계속 듣는다(대화는 안 끝남,
 * 11.5절 "끼어들기"). 그 밖(실시간, 또는 절약 모드지만 말하는 중이 아님)에는 live_voice_stop()과 같다.
 */
void      live_voice_barge_in(void);

bool          live_voice_active(void);      /* 세션 태스크가 살아 있음 */
bool          live_voice_ready(void);       /* session.started 이후, 종료 요청 전 */
voice_state_t live_voice_state(void);
/** 마지막 상태 문구 (고정 문자열, 진단용) */
const char   *live_voice_status(void);
/** 이번 세션 번호. 툴 결과가 이전 세션으로 가지 않게 확인하는 데 쓴다 */
unsigned      live_voice_generation(void);

/**
 * 다음 세션 한 번에만 쓸 시간 제한(ms). 0이면 기본값(최대 10분, 무응답 60초).
 * live_voice_start() 전에 부른다 (영어 수업)
 */
void      live_voice_set_next_limits(uint32_t max_ms, uint32_t idle_ms);

/** 모델에게 지시를 덧붙인다 (마이크 보정 안내 등) */
bool      live_voice_instruction(const char *text);
/** 사용자 메시지를 글로 보낸다 (최대 600바이트) */
esp_err_t live_voice_text(const char *text);

/**
 * 툴 결과를 보낸다. outputs는 {"call_id": "...", "output": "<JSON 문자열>"} 배열이고
 * 소유권을 넘긴다. 다 보낸 뒤 response.create를 보낸다.
 */
esp_err_t live_voice_submit_tool_outputs(cJSON *outputs, unsigned generation);

/**
 * 절약 모드 + Gemini(절약·실시간) 순수 로직 셀프테스트. 통과하면 0x3F (bit0 endpoint, bit1 text,
 * bit2 resample, bit3 gemini_schema, bit4 gemini_base64, bit5 gemini_live_proto).
 * docs/design-voice-saver-mode.md 11.9·16.5절, 3단계(Gemini Live).
 */
unsigned saver_voice_selftest(void);

#ifdef __cplusplus
}
#endif
