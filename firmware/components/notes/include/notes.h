/**
 * @file notes.h
 * @brief 음성 메모·일정 + 일정 알림 (설계서 docs/design-notes-schedule.md 2판, 4·5·8절)
 *
 * 원본은 스피커(/storage/notes.jsonl), 가족 폰은 LAN API로 전체 사본을 맞춘다(rev 기준).
 * 데이터는 모두 PSRAM에 둔다(내부 RAM 여유가 매우 작다). 새 태스크는 만들지 않는다:
 * 알림 검사는 main.c가 30초 esp_timer → APP_EVT_NOTES_TICK으로 이 모듈의 notes_tick()을 부른다.
 *
 * 제목·내용·검색어는 어떤 로그에도 남기지 않는다. 로그는 id·rev·개수만.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** /storage/notes.jsonl·notes.meta를 읽어 PSRAM에 올리고, NVS "notes"/"cfg" 알림 설정을 읽는다 */
esp_err_t notes_init(void);

/* ── LAN API (local_api.c) ────────────────────────────────────────────── */

/**
 * since 이후 바뀐 항목(지운 기록 포함, rev 오름차순) + 현재 rev를 담은 JSON 본문 텍스트.
 * PSRAM 버퍼(호출한 쪽이 free). 메모리가 없으면 NULL.
 */
char *notes_api_since_text(uint32_t since);

/** 새로 만들기. ESP_OK(note_out 채움) | ESP_ERR_INVALID_ARG | ESP_ERR_INVALID_SIZE(1,000건 초과, FULL) */
esp_err_t notes_api_create(const cJSON *body, cJSON **note_out);

/**
 * 고치기. ESP_OK | ESP_ERR_NOT_FOUND(없음·gone) | ESP_ERR_INVALID_ARG
 * | ESP_ERR_INVALID_STATE(409 CONFLICT, conflict_out에 최신 항목을 채운다)
 */
esp_err_t notes_api_update(const char *id, const cJSON *body, cJSON **note_out, cJSON **conflict_out);

/** 원본 영구 삭제(gone). ESP_OK(result_out = {"id","rev","state":"gone"}) | ESP_ERR_NOT_FOUND */
esp_err_t notes_api_delete(const char *id, cJSON **result_out);

/** 휴지통에서 되살리기. ESP_OK | ESP_ERR_NOT_FOUND(없음·gone) */
esp_err_t notes_api_restore(const char *id, cJSON **note_out);

/** 알림 설정 (호출한 쪽이 cJSON_Delete) */
cJSON    *notes_settings_get(void);
/** 부분 수정. ESP_OK | ESP_ERR_INVALID_ARG */
esp_err_t notes_settings_set(const cJSON *body);

/* ── 비서 툴 (assistant.c) ────────────────────────────────────────────── */
/* 모두: 문제가 있으면 모델에게 줄 오류 문구, 되면 NULL (out에 결과를 채운다) */
const char *notes_tool_add(const cJSON *args, cJSON *out);
const char *notes_tool_search(const cJSON *args, cJSON *out);
const char *notes_tool_update(const cJSON *args, cJSON *out);
const char *notes_tool_delete(const cJSON *args, cJSON *out);
const char *notes_tool_snooze(const cJSON *args, cJSON *out);

/* ── 알림 엔진 ────────────────────────────────────────────────────────── */

/** 30초마다(main.c APP_EVT_NOTES_TICK): 알릴 것·놓친 것을 계산한다. 시각이 유효하지 않으면 건너뛴다 */
void notes_tick(void);
/** 말해 줄 묶음이 기다리고 있다(아직 확정 전) */
bool notes_reminder_pending(void);
/**
 * 대화가 준비됐을 때 한 번: 묶음을 "보냄"으로 확정하고(rev 증가, WS notes·reminder 이벤트) 말할 글을 돌려준다
 * (600바이트 이하, 호출한 쪽이 free). 기다리는 것이 없으면 NULL.
 */
char *notes_reminder_build_text(void);
/** 알림 대화 중 사용자가 처음 말했을 때(메아리 아님) 한 번 부른다. 확정된 묶음을 "확인됨"으로 */
void notes_reminder_acked(void);
/** 알림 대화가 끝났을 때: 아직 확인되지 않은 항목은 "대답 없음"으로 확정하고 묶음을 비운다 */
void notes_reminder_session_end(void);
/** 대화를 열 수 없을 때(인터넷 없음·API 키 없음): 묶음을 "놓침"으로 남기고 비운다(다시 울리지 않음) */
void notes_reminder_unreachable(void);

/* 순수 로직 셀프테스트(날짜·UTF-8·검색·id)는 notes_logic.h의 notes_selftest() (ESP-IDF 없이도 cc로 컴파일됨) */

/**
 * notes.c의 알림 상태 전이(놓친 시각 계산, 만들 때 이미 지난 알림을 reminded로 미리 채우기, 30분 초과 → missed)
 * 셀프테스트. 플래시를 건드리지 않지만 ESP-IDF(heap_caps 등)에 기대므로 기기에서만 돈다.
 */
bool notes_state_selftest(void);

#ifdef __cplusplus
}
#endif
