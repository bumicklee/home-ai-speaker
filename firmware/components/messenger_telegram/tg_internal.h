/* messenger_telegram 내부 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "messenger_telegram.h"

esp_err_t tg_store_init(void);
bool      tg_chat_registered(int64_t chat_id, char alias_out[TG_ALIAS_MAX + 1]);
/** 새 대화방 등록 (같은 chat_id면 갱신). 10개가 차면 ESP_ERR_NO_MEM */
esp_err_t tg_chat_add(int64_t chat_id, const char *title, const char *type);
/** 그룹 → 슈퍼그룹 이전 (TLG-05) */
void      tg_chat_migrate(int64_t old_id, int64_t new_id);

int64_t tg_last_update_id(void);
void    tg_save_last_update_id(int64_t id, bool force);
