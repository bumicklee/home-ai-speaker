/*
 * Spotify를 끈 빌드(CONFIG_APP_SPOTIFY=n, 기본)의 대체 코드. 2026-10-03 사용자 결정.
 * 공개 API는 spotify_api.c와 같고, 항상 "연결 안 됨"으로 답한다.
 * 자원을 쓰지 않는다: 잠금·태스크·HTTPS·메모리 할당 없음.
 * - spotify_linked()가 false라 비서 툴·지시문에 Spotify가 나오지 않고, 음악 제어는 라디오로 간다
 * - BLE spotify.link는 ESP_ERR_NOT_SUPPORTED (응답 UNKNOWN_OP)
 * - spotify.logout은 예전에 저장된 값이 있으면 지운다 (켜져 있던 빌드에서 넘어온 경우)
 */
#include "media_spotify.h"

#include "esp_log.h"
#include "secret_store.h"

static const char *TAG = "SPOTIFY";

esp_err_t media_spotify_init(void)
{
    ESP_LOGI(TAG, "disabled (CONFIG_APP_SPOTIFY=n)");
    return ESP_OK;
}

esp_err_t spotify_link(const char *client_id, const char *refresh_token)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t spotify_logout(void)
{
    esp_err_t a = secret_erase(SECRET_SP_CLIENT_ID);
    esp_err_t b = secret_erase(SECRET_SP_REFRESH);
    esp_err_t c = secret_erase(SECRET_SP_CONNECT_BLOB);
    return a != ESP_OK ? a : (b != ESP_OK ? b : c);
}

bool spotify_linked(void)
{
    return false;
}

spotify_state_t spotify_state(void)
{
    return SPOTIFY_OFF;
}

const char *spotify_state_name(spotify_state_t state)
{
    static const char *const names[] = {"off", "ok", "relogin_required", "premium_required", "error"};
    return (unsigned)state < 5 ? names[state] : "off";
}

esp_err_t spotify_play(const char *query, const char *type, char *desc, size_t desc_len)
{
    if (desc && desc_len) {
        desc[0] = '\0';
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t spotify_control(const char *action)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static cJSON *now_none(void)
{
    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "source", "none");
    cJSON_AddStringToObject(j, "state", "stopped");
    return j;
}

cJSON *spotify_now_json(void)
{
    return now_none();
}

cJSON *spotify_now_cached(void)
{
    return now_none();
}

cJSON *spotify_status_json(void)
{
    cJSON *j = cJSON_CreateObject();
    cJSON_AddBoolToObject(j, "linked", false);
    cJSON_AddStringToObject(j, "state", "off");
    cJSON_AddBoolToObject(j, "available", false);     /* 이 펌웨어에는 Spotify가 없다 (앱이 화면을 숨긴다) */
    return j;
}

bool spotify_recently_playing(void)
{
    return false;
}
