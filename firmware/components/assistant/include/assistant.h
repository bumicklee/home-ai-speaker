/* Based on waveshare-ai-speaker-assistant (MIT, Copyright (c) 2026 KMX415). See THIRD_PARTY_NOTICES.md. */
/**
 * @file assistant.h
 * @brief 비서 설정·기억, 프롬프트(session.start) 생성, 툴 실행 (설계서 2.8절)
 *
 * 기존 펌웨어 assistant.c를 옮겼다. live_voice에 핸들러를 등록해
 * 세션 시작 이벤트를 만들어 주고, 툴 호출은 툴 워커 태스크에서 실행한다.
 */
#pragma once

#include <stdbool.h>
#include "cJSON.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 설정·기억을 NVS에서 읽고, 툴 워커와 마이크 처리를 시작하고, live_voice 핸들러를 등록한다 */
esp_err_t assistant_init(void);

/** 현재 설정 복사본 (호출한 쪽이 cJSON_Delete) */
cJSON    *assistant_settings(void);
esp_err_t assistant_save(const cJSON *settings);
cJSON    *assistant_memories(void);
/** 고를 수 있는 OpenAI 보이스 이름 배열 (호출한 쪽이 cJSON_Delete) */
cJSON    *assistant_voice_list(void);
esp_err_t assistant_forget(const char *key);

/** session.start JSON (호출한 쪽이 free) */
char     *assistant_start_event(void);
/**
 * 새 Telegram 메시지를 읽어 주는 짧은 대화를 연다 (TLG-10, 자동 읽기).
 * 대화·영어 수업 중이거나 시작할 수 없으면 ESP_ERR_INVALID_STATE 등
 */
esp_err_t assistant_announce_telegram(int unread);
/**
 * 일정 알림 대화를 연다(notes 모듈이 묶음을 들고 있을 때 main.c가 부른다). 대화·영어 수업 중이거나
 * 시작할 수 없으면 ESP_ERR_INVALID_STATE(busy, 나중에 다시 시도). 그 밖의 실패는 인터넷·API 키 문제로 본다.
 */
esp_err_t assistant_announce_reminders(void);
/** 툴 하나 실행. {"ok": bool, ...} 결과 (호출한 쪽이 cJSON_Delete) */
cJSON    *assistant_execute(const char *name, const char *arguments);

/** 기기가 모델에게 대답을 시켰다 (툴 결과·자동 읽기). 이 차례에 나오는 소리 동안의 입력은 메아리로 본다 */
void      assistant_note_device_turn(void);
/* 툴 워커 (tool_worker.c) */
esp_err_t assistant_tool_worker_start(void);
void      assistant_queue_tool_calls(cJSON *calls, unsigned generation);
/**
 * 막는 작업(HTTPS 등)을 툴 워커에서 실행한다. LAN 핸들러가 웹 서버 태스크를 막지 않게 쓴다.
 * 큐가 차면 false (fn은 불리지 않으므로 arg는 호출한 쪽이 정리)
 */
bool      assistant_queue_work(void (*fn)(void *arg), void *arg);

/** 설정 13키·옛 설정(9·10키) 옮기기 셀프테스트. 통과하면 true (docs/design-voice-saver-mode.md 11.9) */
bool      assistant_settings_migration_selftest(void);

#ifdef __cplusplus
}
#endif
