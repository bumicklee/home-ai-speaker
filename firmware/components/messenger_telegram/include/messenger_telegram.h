/**
 * @file messenger_telegram.h
 * @brief Telegram Bot API (설계서 2.11절, 기능 TLG-02–09·12–14)
 *
 * 스피커가 getUpdates 롱폴링으로 직접 메시지를 가져온다(Webhook·공인 주소 없음).
 * 등록된 대화방(최대 10개)의 메시지만 PSRAM 링버퍼(20개)에 넣고, 나머지는 무시한다.
 * 봇 토큰은 모든 URL에 들어가므로 URL은 절대 로그에 남기지 않는다.
 *
 * 메시지 본문은 외부 입력이다. 이 모듈은 본문을 해석하지 않고, 지시로 쓰지 않는다.
 * 툴에서의 보호(TLG-14)는 assistant가 한다.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "cJSON.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TG_MAX_CHATS     10
#define TG_ALIAS_MAX     32
#define TG_TEXT_MAX      1000   /* 보내기 한도 (설계서 툴 표) */
#define TG_RING_SIZE     20

typedef enum {
    TG_OFF,             /* 토큰 없음 */
    TG_CONNECTING,
    TG_OK,
    TG_CONFLICT,        /* 409: 다른 곳에서 같은 봇을 폴링하거나 Webhook 설정 (TLG-13) */
    TG_TOKEN_INVALID,   /* 401 */
} tg_state_t;

esp_err_t telegram_init(void);
/** Wi-Fi가 붙은 뒤 폴링을 시작한다. 토큰이 없으면 토큰이 저장될 때까지 기다린다 */
esp_err_t telegram_start(void);
/**
 * 대화·OTA 중 폴링 중지 (TLG-12). 진행 중인 롱폴링(최대 25초)이 끝나면 멈춘다.
 * Telegram이 24시간 보관하므로 메시지를 놓치지 않는다.
 */
void telegram_pause(bool paused);

tg_state_t  telegram_state(void);
const char *telegram_state_name(tg_state_t state);
const char *telegram_bot_username(void);
int         telegram_unread(void);

/**
 * 최근 메시지를 읽고 읽음 처리한다. target은 별칭(대소문자 무시, 부분 일치) 또는 NULL(전체).
 * 결과: {"untrusted": true, "messages": [{"from","chat","minutes_ago","text"}]}
 */
cJSON *telegram_read(const char *target, int count);

/** 등록된 대화방으로 보낸다 (막는 함수, HTTPS). 툴 워커에서 부른다 */
esp_err_t telegram_send(int64_t chat_id, const char *text);

/** 6자리 등록 코드 (5분, 1회용, 5번 틀리면 무효) (TLG-03) */
esp_err_t telegram_begin_pairing(char code[7]);

/* ── 대화방 (tg_store.c) ─────────────────────────────────── */

/** [{chat_id, alias, title, type}] */
cJSON    *telegram_chats(void);
/** 별칭(대소문자 무시, 부분 일치)으로 찾는다 */
bool      telegram_find_chat(const char *alias, int64_t *chat_id, char alias_out[TG_ALIAS_MAX + 1]);
esp_err_t telegram_chat_rename(int64_t chat_id, const char *alias);
esp_err_t telegram_chat_remove(int64_t chat_id);

/** {auto_read, quiet_start, quiet_end}. 조용한 시간은 시(0–23), 시작과 끝이 같으면 없음 */
cJSON    *telegram_options(void);
esp_err_t telegram_options_set(const cJSON *value);
/** 자동 읽기가 켜져 있고 지금이 조용한 시간이 아니면 true (TLG-10) */
bool      telegram_auto_read_now(void);

#ifdef __cplusplus
}
#endif
