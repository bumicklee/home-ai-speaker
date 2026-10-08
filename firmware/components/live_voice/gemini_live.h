/**
 * @file gemini_live.h
 * @brief live_voice.c ↔ gemini_live.c 전용 내부 계약 (docs/design-voice-saver-mode.md 3단계: Gemini 실시간)
 *
 * components/live_voice 안에서만 보인다 (CMakeLists의 PRIV_INCLUDE_DIRS "."). live_voice.c가 세션 공유
 * 상태(바쁨·상태·준비·세대·정지 요청)를 들고 있고, gemini_live.c는 saver_voice.c와 같은 식으로 그 상태를
 * saver_voice.h의 voice_shared_* 전달 함수로만 건드린다(새로 만들지 않고 그대로 재사용). 실시간이므로
 * esp_websocket_client를 쓰고 자기 세션 태스크를 따로 갖는다(gpt-live-1과 같은 구조: 그 태스크가 내부
 * RAM 스택을 쓰는 자기 태스크를 또 만들므로 ws_stack_reserve/release 요령이 필요하다 —
 * voice_shared_reserve_release/take로 공유, 한 번에 하나의 WebSocket만).
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 부팅 때 한 번: 세션 태스크를 만든다 (스택 PSRAM, esp_websocket_client 내부 태스크는 내부 RAM) */
esp_err_t gemini_live_init(void);
/** live_voice_start()가 세션 태스크를 깨울 때 쓰는 태스크 진입점 (ulTaskNotifyTake 루프) */
void      gemini_live_task_entry(void *arg);
/** 지금 Gemini Live 세션이 듣고 있는가(setupComplete 이후, 끝나기 전) — live_voice_text/instruction 라우팅용 */
bool      gemini_live_active(void);
/**
 * 알림 문장·텔레그램 자동 읽기·마이크 보정 안내 등 기기가 보내는 글. clientContent(role user,
 * turnComplete true)로 보낸다(gemini_live_proto.h, 요청 5: 지시문 덧붙이기 메시지가 없어서).
 * 세션이 없거나 아직 준비되지 않았으면 ESP_ERR_INVALID_STATE.
 */
esp_err_t gemini_live_text(const char *text);

#ifdef __cplusplus
}
#endif
