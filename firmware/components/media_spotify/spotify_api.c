/*
 * Spotify Web API 클라이언트 (설계서 2.10절). 설명은 media_spotify.h.
 *
 * 토큰:
 *   - 앱이 PKCE로 로그인해 refresh token을 BLE(spotify.link)로 넘긴다. 클라이언트 비밀은 없다(공개 클라이언트).
 *   - access token(1시간)은 RAM에만 두고, 만료 60초 전이나 401이면 갱신한다.
 *   - 갱신 응답에 새 refresh token이 오면 즉시 바꿔 저장한다(토큰 회전).
 *   - refresh token이 만료되면(처음 인증에서 6개월) 400 → RELOGIN_REQUIRED, 앱이 다시 로그인을 안내한다.
 * 모든 Web API 호출은 한 번에 하나씩(뮤텍스). 툴 워커, BLE 작업 태스크, LAN 핸들러가 부른다.
 */
#include "media_spotify.h"

#include <ctype.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "app_config.h"
#include "app_events.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "secret_store.h"

static const char *TAG = "SPOTIFY";

#define ACCOUNTS_TOKEN  "https://accounts.spotify.com/api/token"
#define API_BASE        "https://api.spotify.com/v1"
#define RESPONSE_MAX    (32 * 1024)
#define TOKEN_MAX       512
#define REFRESH_MARGIN  (60LL * 1000 * 1000)
#define HTTP_TIMEOUT_MS 10000

static SemaphoreHandle_t s_lock;
static char s_access[TOKEN_MAX];
static int64_t s_access_expires_us;
static atomic_int s_state = SPOTIFY_OFF;
static char s_account[64];
static char s_device[64];
static char s_last_error[64];
static atomic_bool s_recently_playing;
/* 표시용 문자열과 now 캐시는 긴 HTTPS 잠금(s_lock)과 따로 짧게 잠근다 (LAN 핸들러가 기다리지 않게) */
static portMUX_TYPE s_info_mux = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t s_cache_lock;
static cJSON *s_now_cache;

static void info_set(char *dst, size_t len, const char *value)
{
    portENTER_CRITICAL(&s_info_mux);
    strlcpy(dst, value ? value : "", len);
    portEXIT_CRITICAL(&s_info_mux);
}

static void info_get(char *out, size_t len, const char *src)
{
    portENTER_CRITICAL(&s_info_mux);
    strlcpy(out, src, len);
    portEXIT_CRITICAL(&s_info_mux);
}

/* ── 공통 ─────────────────────────────────────────────────── */

const char *spotify_state_name(spotify_state_t state)
{
    static const char *const names[] = {"off", "ok", "relogin_required", "premium_required", "error"};
    return (unsigned)state < 5 ? names[state] : "off";
}

spotify_state_t spotify_state(void)
{
    return atomic_load(&s_state);
}

bool spotify_linked(void)
{
    return secret_exists(SECRET_SP_REFRESH) && secret_exists(SECRET_SP_CLIENT_ID);
}

bool spotify_recently_playing(void)
{
    return atomic_load(&s_recently_playing);
}

static void set_error(spotify_state_t state, const char *error)
{
    atomic_store(&s_state, state);
    info_set(s_last_error, sizeof(s_last_error), error);
}

static void url_encode(const char *in, char *out, size_t out_len)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t o = 0;
    for (; *in && o + 4 < out_len; in++) {
        unsigned char c = (unsigned char)*in;
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out[o++] = (char)c;
        } else {
            out[o++] = '%';
            out[o++] = hex[c >> 4];
            out[o++] = hex[c & 15];
        }
    }
    out[o] = '\0';
}

typedef struct {
    char *buf;
    size_t used;
} body_t;

static esp_err_t on_http(esp_http_client_event_t *evt)
{
    body_t *b = evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA && b && b->used + evt->data_len < RESPONSE_MAX) {
        memcpy(b->buf + b->used, evt->data, evt->data_len);
        b->used += evt->data_len;
    }
    return ESP_OK;
}

/* HTTP 요청 하나. 상태 코드(네트워크 오류면 -1). out에 응답 JSON(없으면 NULL) */
static int http_request(esp_http_client_method_t method, const char *url, const char *auth, const char *content_type,
                        const char *body, cJSON **out)
{
    *out = NULL;
    body_t resp = {.buf = heap_caps_malloc(RESPONSE_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)};
    if (!resp.buf) {
        return -1;
    }
    const esp_http_client_config_t cfg = {
        .url = url,
        .method = method,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .event_handler = on_http,
        .user_data = &resp,
        .buffer_size = 2048,
        .buffer_size_tx = 2048,     /* Authorization 헤더가 길다 */
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    int status = -1;
    if (client) {
        if (auth) {
            esp_http_client_set_header(client, "Authorization", auth);
        }
        if (content_type) {
            esp_http_client_set_header(client, "Content-Type", content_type);
        }
        if (body) {     /* 본문이 없으면 부르지 않는다 (부르면 Content-Type이 붙는다) */
            esp_http_client_set_post_field(client, body, strlen(body));
        }
        if (esp_http_client_perform(client) == ESP_OK) {
            status = esp_http_client_get_status_code(client);
        }
        esp_http_client_cleanup(client);
    }
    if (status > 0 && resp.used) {
        resp.buf[resp.used] = '\0';
        *out = cJSON_Parse(resp.buf);
    }
    memset(resp.buf, 0, resp.used);     /* 토큰 응답일 수 있다 */
    free(resp.buf);
    return status;
}

/* ── 토큰 ─────────────────────────────────────────────────── */

/* client_id·refresh token으로 access token을 받는다. 새 refresh token이 오면 rotated에 넣는다 */
static esp_err_t token_request(const char *client_id, const char *refresh, char *rotated, size_t rotated_len)
{
    const size_t enc_cap = TOKEN_MAX * 3;
    char *enc_refresh = malloc(enc_cap), enc_id[100];
    size_t form_len = enc_cap + sizeof(enc_id) + 64;
    char *form = malloc(form_len);
    if (!form || !enc_refresh) {
        free(form);
        free(enc_refresh);
        return ESP_ERR_NO_MEM;
    }
    url_encode(refresh, enc_refresh, enc_cap);
    url_encode(client_id, enc_id, sizeof(enc_id));
    snprintf(form, form_len, "grant_type=refresh_token&refresh_token=%s&client_id=%s", enc_refresh, enc_id);
    memset(enc_refresh, 0, enc_cap);
    free(enc_refresh);
    cJSON *resp = NULL;
    int status = http_request(HTTP_METHOD_POST, ACCOUNTS_TOKEN, NULL, "application/x-www-form-urlencoded", form, &resp);
    memset(form, 0, form_len);
    free(form);

    esp_err_t err;
    const cJSON *access = cJSON_GetObjectItem(resp, "access_token");
    const cJSON *expires = cJSON_GetObjectItem(resp, "expires_in");
    const cJSON *next = cJSON_GetObjectItem(resp, "refresh_token");
    if (status == 200 && cJSON_IsString(access) && strlen(access->valuestring) < TOKEN_MAX) {
        strlcpy(s_access, access->valuestring, sizeof(s_access));
        s_access_expires_us = esp_timer_get_time() + (int64_t)(cJSON_IsNumber(expires) ? expires->valueint : 3600) * 1000000;
        if (rotated) {
            rotated[0] = '\0';
            if (cJSON_IsString(next) && strlen(next->valuestring) < rotated_len) {
                strlcpy(rotated, next->valuestring, rotated_len);
            }
        }
        err = ESP_OK;
    } else if (status == 400 || status == 401) {
        err = ESP_ERR_INVALID_ARG;      /* invalid_grant: 만료·해지 또는 잘못된 값 */
    } else {
        err = ESP_FAIL;
    }
    if (resp) {
        if (cJSON_IsString(access)) {
            memset(access->valuestring, 0, strlen(access->valuestring));
        }
        if (cJSON_IsString(next)) {
            memset(next->valuestring, 0, strlen(next->valuestring));
        }
        cJSON_Delete(resp);
    }
    ESP_LOGI(TAG, "token refresh: HTTP %d", status);
    return err;
}

/* 저장된 값으로 갱신 (잠금을 잡은 상태에서) */
static esp_err_t refresh_locked(void)
{
    char client_id[64], refresh[TOKEN_MAX], rotated[TOKEN_MAX];
    size_t id_len = sizeof(client_id) - 1, rf_len = sizeof(refresh) - 1;
    if (secret_get(SECRET_SP_CLIENT_ID, client_id, &id_len) != ESP_OK ||
        secret_get(SECRET_SP_REFRESH, refresh, &rf_len) != ESP_OK) {
        set_error(SPOTIFY_OFF, "");
        return ESP_ERR_INVALID_STATE;
    }
    client_id[id_len] = '\0';
    refresh[rf_len] = '\0';
    esp_err_t err = token_request(client_id, refresh, rotated, sizeof(rotated));
    if (err == ESP_OK && rotated[0] && strcmp(rotated, refresh)) {
        secret_set(SECRET_SP_REFRESH, rotated, strlen(rotated));   /* 토큰 회전 */
    }
    memset(refresh, 0, sizeof(refresh));
    memset(rotated, 0, sizeof(rotated));
    if (err == ESP_ERR_INVALID_ARG) {
        set_error(SPOTIFY_RELOGIN_REQUIRED, "refresh token expired or revoked");
    } else if (err != ESP_OK) {
        set_error(SPOTIFY_ERROR, "token request failed");
    }
    return err;
}

/* Web API 호출. 토큰이 없거나 곧 만료되면 갱신하고, 401이면 한 번 더 갱신해 다시 시도한다 */
static int api_locked(esp_http_client_method_t method, const char *path, const char *json, cJSON **out)
{
    *out = NULL;
    if ((!s_access[0] || esp_timer_get_time() > s_access_expires_us - REFRESH_MARGIN) && refresh_locked() != ESP_OK) {
        return 0;
    }
    char url[512];
    snprintf(url, sizeof(url), API_BASE "%s", path);
    char auth[TOKEN_MAX + 8];
    int status = 0;
    /* 본문 없는 POST·PUT(next, pause 등)도 Content-Length가 붙도록 빈 객체를 보낸다 */
    if (!json && method != HTTP_METHOD_GET) {
        json = "{}";
    }
    for (int attempt = 0; attempt < 2; attempt++) {
        snprintf(auth, sizeof(auth), "Bearer %s", s_access);
        status = http_request(method, url, auth, json ? "application/json" : NULL, json, out);
        if (status != 401 || attempt == 1) {
            break;
        }
        cJSON_Delete(*out);
        *out = NULL;
        if (refresh_locked() != ESP_OK) {
            break;
        }
    }
    memset(auth, 0, sizeof(auth));
    return status;
}

static const char *error_reason(const cJSON *resp)
{
    const cJSON *reason = cJSON_GetObjectItem(cJSON_GetObjectItem(resp, "error"), "reason");
    return cJSON_IsString(reason) ? reason->valuestring : "";
}

/* 재생 명령의 결과 상태 → esp_err. Premium 여부는 여기서만 알 수 있다 (GET /me에 product가 없음) */
static esp_err_t player_result(int status, const cJSON *resp)
{
    if (status >= 200 && status < 300) {
        atomic_store(&s_state, SPOTIFY_OK);
        info_set(s_last_error, sizeof(s_last_error), "");
        return ESP_OK;
    }
    if (status == 403 && !strcmp(error_reason(resp), "PREMIUM_REQUIRED")) {
        set_error(SPOTIFY_PREMIUM_REQUIRED, "premium required");
        return ESP_ERR_NOT_ALLOWED;
    }
    if (status == 404) {
        set_error(SPOTIFY_OK, "no active device");
        return ESP_ERR_INVALID_STATE;
    }
    if (status == 0) {
        return ESP_ERR_INVALID_STATE;   /* 연결 안 됨 / 다시 로그인 필요 */
    }
    set_error(SPOTIFY_ERROR, "player command failed");
    return ESP_FAIL;
}

/* ── 연결 ─────────────────────────────────────────────────── */

static bool client_id_valid(const char *id)
{
    if (!id || strlen(id) != 32) {
        return false;
    }
    for (const char *p = id; *p; p++) {
        if (!isxdigit((unsigned char)*p)) {
            return false;
        }
    }
    return true;
}

static void fetch_account_locked(void)
{
    cJSON *me = NULL;
    if (api_locked(HTTP_METHOD_GET, "/me", NULL, &me) == 200) {
        const cJSON *name = cJSON_GetObjectItem(me, "display_name");
        info_set(s_account, sizeof(s_account), cJSON_IsString(name) ? name->valuestring : "");
    }
    cJSON_Delete(me);
}

static void secrets_changed(void)
{
    const app_setting_t key = APP_SETTING_SECRETS;
    app_events_post(APP_EVT_SETTINGS_CHANGED, &key, sizeof(key), 0);
}

esp_err_t spotify_link(const char *client_id, const char *refresh_token)
{
    if (!client_id_valid(client_id) || !refresh_token || !refresh_token[0] || strlen(refresh_token) >= TOKEN_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    /* 저장하기 전에 실제로 갱신이 되는지 확인한다 (설계서 2.13절 spotify.link) */
    char rotated[TOKEN_MAX];
    esp_err_t err = token_request(client_id, refresh_token, rotated, sizeof(rotated));
    if (err == ESP_OK) {
        const char *keep = rotated[0] ? rotated : refresh_token;
        err = secret_set(SECRET_SP_CLIENT_ID, client_id, strlen(client_id));
        if (err == ESP_OK) {
            err = secret_set(SECRET_SP_REFRESH, keep, strlen(keep));
        }
        if (err == ESP_OK) {
            atomic_store(&s_state, SPOTIFY_OK);
            info_set(s_last_error, sizeof(s_last_error), "");
            fetch_account_locked();
        }
    } else {
        s_access[0] = '\0';
        if (err == ESP_ERR_INVALID_ARG) {
            ESP_LOGW(TAG, "link rejected by Spotify");
        }
    }
    memset(rotated, 0, sizeof(rotated));
    xSemaphoreGive(s_lock);
    if (err == ESP_OK) {
        secrets_changed();
    }
    return err;
}

esp_err_t spotify_logout(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t a = secret_erase(SECRET_SP_CLIENT_ID);
    esp_err_t b = secret_erase(SECRET_SP_REFRESH);
    esp_err_t c = secret_erase(SECRET_SP_CONNECT_BLOB);
    memset(s_access, 0, sizeof(s_access));
    info_set(s_account, sizeof(s_account), "");
    info_set(s_device, sizeof(s_device), "");
    set_error(SPOTIFY_OFF, "");
    atomic_store(&s_recently_playing, false);
    xSemaphoreGive(s_lock);
    secrets_changed();
    return a != ESP_OK ? a : (b != ESP_OK ? b : c);
}

/* ── 재생 ─────────────────────────────────────────────────── */

/*
 * 재생할 기기: 이 스피커의 Connect 수신기(이름이 APP_SPOTIFY_DEVICE_NAME으로 시작) → 지금 활성 기기
 * → 제한되지 않은 첫 기기. 수신기를 붙이기 전(Q2)에는 휴대폰 등 같은 계정의 기기가 고른다.
 */
static bool pick_device_locked(char *id, size_t id_len)
{
    cJSON *resp = NULL;
    int status = api_locked(HTTP_METHOD_GET, "/me/player/devices", NULL, &resp);
    const cJSON *chosen = NULL, *active = NULL, *first = NULL, *d;
    cJSON_ArrayForEach(d, cJSON_GetObjectItem(resp, "devices")) {
        const cJSON *name = cJSON_GetObjectItem(d, "name");
        if (cJSON_IsTrue(cJSON_GetObjectItem(d, "is_restricted")) || !cJSON_IsString(cJSON_GetObjectItem(d, "id"))) {
            continue;
        }
        if (cJSON_IsString(name) && !strncasecmp(name->valuestring, APP_SPOTIFY_DEVICE_NAME, strlen(APP_SPOTIFY_DEVICE_NAME))) {
            chosen = d;
            break;
        }
        if (!active && cJSON_IsTrue(cJSON_GetObjectItem(d, "is_active"))) {
            active = d;
        }
        if (!first) {
            first = d;
        }
    }
    if (!chosen) {
        chosen = active ? active : first;
    }
    bool ok = status == 200 && chosen;
    if (ok) {
        strlcpy(id, cJSON_GetObjectItem(chosen, "id")->valuestring, id_len);
        const cJSON *name = cJSON_GetObjectItem(chosen, "name");
        info_set(s_device, sizeof(s_device), cJSON_IsString(name) ? name->valuestring : "");
    }
    cJSON_Delete(resp);
    return ok;
}

static const char *first_artist(const cJSON *item)
{
    const cJSON *a = cJSON_GetObjectItem(cJSON_GetArrayItem(cJSON_GetObjectItem(item, "artists"), 0), "name");
    return cJSON_IsString(a) ? a->valuestring : "";
}

esp_err_t spotify_play(const char *query, const char *type, char *desc, size_t desc_len)
{
    static const char *const types[] = {"track", "artist", "album", "playlist"};
    bool type_ok = false;
    for (int i = 0; i < 4 && type; i++) {
        type_ok |= !strcmp(type, types[i]);
    }
    if (!query || !query[0] || strlen(query) > 100 || !type_ok) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!spotify_linked()) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    char enc[320], path[400];
    url_encode(query, enc, sizeof(enc));
    /* Development Mode 검색 한도는 10개. 하나만 받는다 */
    snprintf(path, sizeof(path), "/search?q=%s&type=%s&limit=1&market=from_token", enc, type);
    cJSON *resp = NULL;
    int status = api_locked(HTTP_METHOD_GET, path, NULL, &resp);
    char key[16];
    snprintf(key, sizeof(key), "%ss", type);    /* tracks, artists, albums, playlists */
    /* 재생목록 결과에는 null 항목이 섞일 수 있다 */
    const cJSON *item = NULL, *it;
    cJSON_ArrayForEach(it, cJSON_GetObjectItem(cJSON_GetObjectItem(resp, key), "items")) {
        if (cJSON_IsObject(it)) {
            item = it;
            break;
        }
    }
    const cJSON *uri = cJSON_GetObjectItem(item, "uri"), *name = cJSON_GetObjectItem(item, "name");
    esp_err_t err;
    if (status != 200) {
        err = player_result(status, resp);
    } else if (!cJSON_IsString(uri)) {
        err = ESP_ERR_NOT_FOUND;
    } else {
        const char *n = cJSON_IsString(name) ? name->valuestring : query;
        if (!strcmp(type, "track") || !strcmp(type, "album")) {
            snprintf(desc, desc_len, "%s - %s", n, first_artist(item));
        } else {
            strlcpy(desc, n, desc_len);
        }
        char device[64];
        if (!pick_device_locked(device, sizeof(device))) {
            err = ESP_ERR_INVALID_STATE;    /* 켜져 있는 Spotify 기기가 없다 */
        } else {
            /* 곡은 uris, 가수·앨범·재생목록은 context_uri (가수 인기곡 API는 2026년에 없어졌다) */
            cJSON *body = cJSON_CreateObject();
            if (!strcmp(type, "track")) {
                cJSON *uris = cJSON_AddArrayToObject(body, "uris");
                cJSON_AddItemToArray(uris, cJSON_CreateString(uri->valuestring));
            } else {
                cJSON_AddStringToObject(body, "context_uri", uri->valuestring);
            }
            char *json = cJSON_PrintUnformatted(body);
            cJSON_Delete(body);
            snprintf(path, sizeof(path), "/me/player/play?device_id=%s", device);
            cJSON *play_resp = NULL;
            int play_status = json ? api_locked(HTTP_METHOD_PUT, path, json, &play_resp) : -1;
            free(json);
            err = player_result(play_status, play_resp);
            cJSON_Delete(play_resp);
        }
    }
    cJSON_Delete(resp);
    if (err == ESP_OK) {
        atomic_store(&s_recently_playing, true);
    }
    xSemaphoreGive(s_lock);
    ESP_LOGI(TAG, "play %s: %s", type, esp_err_to_name(err));   /* 검색어는 남기지 않는다 */
    return err;
}

esp_err_t spotify_control(const char *action)
{
    if (!spotify_linked()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_http_client_method_t method;
    const char *path;
    if (!strcmp(action, "pause") || !strcmp(action, "stop")) {
        method = HTTP_METHOD_PUT;
        path = "/me/player/pause";
    } else if (!strcmp(action, "resume")) {
        method = HTTP_METHOD_PUT;
        path = "/me/player/play";
    } else if (!strcmp(action, "next")) {
        method = HTTP_METHOD_POST;
        path = "/me/player/next";
    } else if (!strcmp(action, "previous")) {
        method = HTTP_METHOD_POST;
        path = "/me/player/previous";
    } else {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    cJSON *resp = NULL;
    int status = api_locked(method, path, NULL, &resp);
    esp_err_t err = player_result(status, resp);
    cJSON_Delete(resp);
    if (err == ESP_OK) {
        atomic_store(&s_recently_playing, strcmp(action, "pause") && strcmp(action, "stop"));
    }
    xSemaphoreGive(s_lock);
    return err;
}

cJSON *spotify_now_json(void)
{
    cJSON *j = cJSON_CreateObject();
    if (!spotify_linked()) {
        cJSON_AddStringToObject(j, "source", "none");
        return j;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    cJSON *resp = NULL;
    int status = api_locked(HTTP_METHOD_GET, "/me/player", NULL, &resp);
    xSemaphoreGive(s_lock);
    const cJSON *item = cJSON_GetObjectItem(resp, "item");
    bool playing = status == 200 && cJSON_IsTrue(cJSON_GetObjectItem(resp, "is_playing"));
    atomic_store(&s_recently_playing, playing);
    if (status != 200 || !cJSON_IsObject(item)) {     /* 204 = 재생 중인 것 없음 */
        cJSON_AddStringToObject(j, "source", "none");
        cJSON_AddStringToObject(j, "state", "stopped");
    } else {
        const cJSON *name = cJSON_GetObjectItem(item, "name");
        const cJSON *album = cJSON_GetObjectItem(cJSON_GetObjectItem(item, "album"), "name");
        const cJSON *device = cJSON_GetObjectItem(cJSON_GetObjectItem(resp, "device"), "name");
        cJSON_AddStringToObject(j, "source", "spotify");
        cJSON_AddStringToObject(j, "state", playing ? "playing" : "paused");
        cJSON_AddStringToObject(j, "title", cJSON_IsString(name) ? name->valuestring : "");
        cJSON_AddStringToObject(j, "artist", first_artist(item));
        cJSON_AddStringToObject(j, "album", cJSON_IsString(album) ? album->valuestring : "");
        cJSON_AddStringToObject(j, "device", cJSON_IsString(device) ? device->valuestring : "");
    }
    cJSON_Delete(resp);
    /* LAN 핸들러가 기다리지 않고 쓰도록 캐시해 둔다 */
    cJSON *copy = cJSON_Duplicate(j, true);
    xSemaphoreTake(s_cache_lock, portMAX_DELAY);
    cJSON_Delete(s_now_cache);
    s_now_cache = copy;
    xSemaphoreGive(s_cache_lock);
    return j;
}

cJSON *spotify_now_cached(void)
{
    xSemaphoreTake(s_cache_lock, portMAX_DELAY);
    cJSON *copy = s_now_cache ? cJSON_Duplicate(s_now_cache, true) : NULL;
    xSemaphoreGive(s_cache_lock);
    if (!copy) {
        copy = cJSON_CreateObject();
        cJSON_AddStringToObject(copy, "source", "none");
        cJSON_AddStringToObject(copy, "state", "stopped");
    }
    return copy;
}

cJSON *spotify_status_json(void)
{
    cJSON *j = cJSON_CreateObject();
    bool linked = spotify_linked();
    cJSON_AddBoolToObject(j, "linked", linked);
    cJSON_AddStringToObject(j, "state", spotify_state_name(linked ? spotify_state() : SPOTIFY_OFF));
    char buf[64];
    info_get(buf, sizeof(buf), s_account);
    cJSON_AddStringToObject(j, "account", buf);
    /* Connect 수신기(스피커 직접 재생)는 Q2 결정 후. 지금은 마지막으로 고른 재생 기기 */
    cJSON_AddBoolToObject(j, "connect_receiver", false);
    info_get(buf, sizeof(buf), s_device);
    cJSON_AddStringToObject(j, "device", buf);
    info_get(buf, sizeof(buf), s_last_error);
    cJSON_AddStringToObject(j, "last_error", buf);
    return j;
}

esp_err_t media_spotify_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_cache_lock = xSemaphoreCreateMutex();
    if (!s_lock || !s_cache_lock) {
        return ESP_ERR_NO_MEM;
    }
    atomic_store(&s_state, spotify_linked() ? SPOTIFY_OK : SPOTIFY_OFF);    /* 첫 호출 때 실제 확인 */
    ESP_LOGI(TAG, "%s", spotify_linked() ? "linked" : "not linked");
    return ESP_OK;
}
