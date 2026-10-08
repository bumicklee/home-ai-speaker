/**
 * @file secret_store.h
 * @brief 비밀값 저장소 (secrets 파티션, 평문 NVS)
 *
 * secret_get 결과는 BLE·LAN 어느 쪽으로도 내보내지 않는다. 앱은 저장 여부만 볼 수 있다.
 * 로그에는 값 대신 길이만 남긴다.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SECRET_OPENAI_KEY,
    SECRET_TG_BOT_TOKEN,
    SECRET_SP_CLIENT_ID,
    SECRET_SP_REFRESH,
    SECRET_SP_CONNECT_BLOB,
    SECRET_STOCK_KEY,
    SECRET_GEMINI_KEY,      /* docs/design-voice-saver-mode.md 11.2 (1~512바이트) */
    SECRET_COUNT,
} secret_id_t;

esp_err_t secret_set(secret_id_t id, const void *value, size_t len);
esp_err_t secret_get(secret_id_t id, void *out, size_t *len);   /* 펌웨어 내부 전용 */
bool      secret_exists(secret_id_t id);
esp_err_t secret_erase(secret_id_t id);

#ifdef __cplusplus
}
#endif
