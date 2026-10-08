/**
 * @file gemini_live_proto.h
 * @brief Gemini Live(BidiGenerateContent) WebSocket 메시지 만들기·읽기 (순수 로직, docs/design-voice-saver-mode.md 3단계)
 *
 * gemini_live.c(ESP-IDF·소켓 담당)가 메시지 모양(키 이름·중첩 구조)을 여기서만 알게 한다. cJSON과
 * gemini_base64만 쓴다(gemini_schema.c·gemini_base64.c처럼 Mac에서 바로 컴파일·시험 가능, ESP-IDF 의존
 * 없음). 메시지 모양은 2026-10 ai.google.dev/api/live, /gemini-api/docs/live-api/get-started-websocket,
 * /gemini-api/docs/live-api/tools, /gemini-api/docs/live-api/capabilities 확인:
 *   - 클라이언트 → 서버: {"setup":{...}} (첫 메시지), {"realtimeInput":{"audio":{"data":b64,
 *     "mimeType":"audio/pcm;rate=16000"}}}, {"clientContent":{"turns":[...],"turnComplete":true}},
 *     {"toolResponse":{"functionResponses":[{"id","name","response"}]}}
 *   - 서버 → 클라이언트: {"setupComplete":{}}, {"serverContent":{"modelTurn":{"parts":[{"inlineData":
 *     {"mimeType":"audio/pcm;rate=24000","data":b64}}]},"turnComplete":bool,"interrupted":bool,
 *     "inputTranscription":{"text":...},"outputTranscription":{"text":...}}},
 *     {"toolCall":{"functionCalls":[{"id","name","args"}]}}, {"toolCallCancellation":{"ids":[...]}},
 *     {"goAway":{"timeLeft":...}}
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 첫 메시지("setup"). model은 "models/gemini-3.8-live" 같은 전체 이름(모델 이름에 "models/"가 이미
 * 있으면 그대로, 없으면 이 함수가 붙인다). voice는 Gemini 목소리 이름(NULL·빈 문자열이면 생략, Gemini
 * 기본값으로 간다). tools_decls는 gemini_schema_convert_tools() 결과 배열(NULL·비어 있으면 함수 도구
 * 없음, 내용은 복사한다). search가 true면 googleSearch 내장 도구를 함수 선언과 함께 더한다(요청 6:
 * Live API는 이 둘을 같이 쓸 수 있다고 문서에 있다 — generateContent REST를 쓰는 절약 모드의 "미리보기
 * 라 뺀다" 결정과는 다르다). inputAudioTranscription·outputAudioTranscription은 항상 켠다(끝내는 말·
 * TLG-14·대화 기록에 받아쓴 글이 필요해서, 11.6/16.5절과 같은 이유). 호출한 쪽이 free, 실패하면 NULL.
 */
char *gemini_live_build_setup(const char *model, const char *voice, const char *instructions,
                               const cJSON *tools_decls, bool search);

/**
 * 마이크 조각 하나(realtimeInput.audio, 16비트 리틀엔디안 16kHz PCM, mimeType "audio/pcm;rate=16000").
 * 호출한 쪽이 free, 실패(메모리 부족)하면 NULL.
 */
char *gemini_live_build_audio_chunk(const int16_t *pcm, size_t sample_count);

/**
 * 기기가 보내는 글(clientContent, role "user", turnComplete true) — live_voice_text()(알림·자동 읽기)와
 * live_voice_instruction()(마이크 보정 안내) 둘 다 이 모양으로 보낸다. Gemini Live에는 OpenAI의
 * "session.instructions.append" 같은 "지시문에 덧붙이기" 전용 메시지가 없어(2026-10 확인), 사용자 차례로
 * 보낸다(요청 5 결정, 문서화). 호출한 쪽이 free, 실패하면 NULL.
 */
char *gemini_live_build_client_text(const char *text);

/**
 * 도구 실행 결과 응답(toolResponse.functionResponses[]). responses는 호출한 쪽이 만든 배열({"id","name",
 * "response"} 객체들, 비어 있으면 안 됨) — 소유권을 넘긴다(이 함수가 cJSON_Delete). 호출한 쪽이 문자열을
 * free, 실패하면 NULL(이때도 responses는 지워진다).
 */
char *gemini_live_build_tool_response(cJSON *responses);

/* ── 서버 메시지 읽기 (root는 cJSON_Parse 결과, 소유권은 호출한 쪽. 아래 함수들은 안을 그대로 가리키는
 * 포인터만 돌려주고 복사하지 않는다 — root가 살아 있는 동안만 쓸 수 있다) ───────────────────────── */

/** {"setupComplete":{}} 메시지인가 (필드가 없어도 키만 있으면 true) */
bool gemini_live_is_setup_complete(const cJSON *root);

/** serverContent 객체 (없으면 NULL) */
const cJSON *gemini_live_server_content(const cJSON *content_holder_root);

bool gemini_live_is_interrupted(const cJSON *content);
bool gemini_live_is_turn_complete(const cJSON *content);

/** 입력(사람, input=true)·출력(비서, input=false) 전사 글 (비어 있지 않을 때만, 없으면 NULL) */
const char *gemini_live_transcript_text(const cJSON *content, bool input);

/** modelTurn.parts[] 중 첫 inlineData(base64 오디오) 글자 포인터와 길이 (없으면 NULL) */
const char *gemini_live_find_inline_audio(const cJSON *content, size_t *len_out);

/** toolCall.functionCalls 배열 (없으면 NULL) */
const cJSON *gemini_live_tool_calls(const cJSON *root);

/** toolCallCancellation.ids 배열 (없으면 NULL) */
const cJSON *gemini_live_cancelled_ids(const cJSON *root);

/** goAway 메시지인가 (연결이 곧 끊긴다는 서버 알림) */
bool gemini_live_is_goaway(const cJSON *root);

bool gemini_live_proto_selftest(void);

#ifdef __cplusplus
}
#endif
