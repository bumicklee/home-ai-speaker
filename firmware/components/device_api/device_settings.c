/*
 * 설정 그룹 JSON ↔ 기기 설정 (설계서 2.14절 /api/v1/settings/{group}, BLE settings.get/set).
 * 부분 수정: 요청에 있는 키만 바꾼다. 하나라도 틀리면 아무것도 바꾸지 않는다.
 */
#include "device_api.h"

#include <string.h>
#include "app_config.h"
#include "app_events.h"
#include "assistant.h"
#include "audio_core.h"
#include "board.h"
#include "mic_control.h"
#include "secret_store.h"
#include "settings.h"
#include "wake.h"

static void notify(app_setting_t key)
{
    app_events_post(APP_EVT_SETTINGS_CHANGED, &key, sizeof(key), 0);
}

/* 정수 값이 있으면 범위를 확인해 out에 넣는다. 없으면 그대로. 타입·범위가 틀리면 false */
static bool take_int(const cJSON *obj, const char *key, int min, int max, int *out)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!v) {
        return true;
    }
    if (!cJSON_IsNumber(v) || v->valuedouble != v->valueint || v->valueint < min || v->valueint > max) {
        return false;
    }
    *out = v->valueint;
    return true;
}

static bool take_bool(const cJSON *obj, const char *key, bool *out)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!v) {
        return true;
    }
    if (!cJSON_IsBool(v)) {
        return false;
    }
    *out = cJSON_IsTrue(v);
    return true;
}

/* 허용된 키만 들어 있는지 */
static bool only_keys(const cJSON *obj, const char *const *keys, size_t n)
{
    if (!cJSON_IsObject(obj)) {
        return false;
    }
    const cJSON *item;
    cJSON_ArrayForEach(item, obj) {
        bool known = false;
        for (size_t i = 0; i < n && !known; i++) {
            known = !strcmp(item->string, keys[i]);
        }
        if (!known) {
            return false;
        }
    }
    return true;
}

/* ── audio ────────────────────────────────────────────────── */

static const char *const AUDIO_KEYS[] = {"volume", "mic_gain", "auto_gain", "duck_db", "spotify_pause_on_voice"};

static cJSON *audio_get(void)
{
    settings_audio_t a;
    settings_get_audio(&a);
    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "volume", audio_core_get_volume());
    cJSON_AddNumberToObject(j, "mic_gain", mic_gain());
    cJSON_AddBoolToObject(j, "auto_gain", mic_auto_gain());
    cJSON_AddNumberToObject(j, "duck_db", a.duck_db);
    cJSON_AddBoolToObject(j, "spotify_pause_on_voice", a.sp_pause_voice);
    return j;
}

static esp_err_t audio_set(const cJSON *v)
{
    settings_audio_t a;
    settings_get_audio(&a);
    int volume = audio_core_get_volume(), gain = mic_gain();
    bool automatic = mic_auto_gain();
    if (!only_keys(v, AUDIO_KEYS, 5) || !take_int(v, "volume", 0, APP_VOLUME_MAX, &volume) ||
        !take_int(v, "mic_gain", 0, 36, &gain) || !take_bool(v, "auto_gain", &automatic) ||
        !take_int(v, "duck_db", 0, 40, &a.duck_db) || !take_bool(v, "spotify_pause_on_voice", &a.sp_pause_voice)) {
        return ESP_ERR_INVALID_ARG;
    }
    /* 덕킹·Spotify 값을 먼저 저장하고, 볼륨·마이크는 적용 함수가 각자 저장한다 */
    a.volume = audio_core_get_volume();
    a.mic_gain = mic_gain();
    a.auto_gain = mic_auto_gain();
    esp_err_t err = settings_set_audio(&a);
    if (err == ESP_OK && volume != audio_core_get_volume()) {
        err = audio_core_set_volume(volume);
    }
    if (err == ESP_OK && (gain != mic_gain() || automatic != mic_auto_gain())) {
        err = mic_set(gain, automatic, true);
    }
    if (err == ESP_OK) {
        notify(APP_SETTING_AUDIO);
    }
    return err;
}

/* ── wake ─────────────────────────────────────────────────── */

static const char *const WAKE_KEYS[] = {"enabled", "phrase", "sensitivity", "gain_db"};

static cJSON *wake_get(void)
{
    settings_wake_t w;
    wake_get_config(&w);
    cJSON *j = cJSON_CreateObject();
    cJSON_AddBoolToObject(j, "enabled", w.enabled);
    cJSON_AddNumberToObject(j, "phrase", w.phrase);     /* 0 Alexa, 1 Computer */
    cJSON_AddNumberToObject(j, "sensitivity", w.sensitivity);
    cJSON_AddNumberToObject(j, "gain_db", w.gain_db);
    return j;
}

static esp_err_t wake_set(const cJSON *v)
{
    settings_wake_t w;
    wake_get_config(&w);
    int phrase = w.phrase, sensitivity = w.sensitivity, gain = w.gain_db;
    if (!only_keys(v, WAKE_KEYS, 4) || !take_bool(v, "enabled", &w.enabled) || !take_int(v, "phrase", 0, 1, &phrase) ||
        !take_int(v, "sensitivity", 0, 100, &sensitivity) || !take_int(v, "gain_db", 0, 24, &gain)) {
        return ESP_ERR_INVALID_ARG;
    }
    w.phrase = phrase;
    w.sensitivity = sensitivity;
    w.gain_db = gain;
    esp_err_t err = wake_set_config(&w);
    if (err == ESP_OK) {
        notify(APP_SETTING_WAKE);
    }
    return err;
}

/* ── assistant ────────────────────────────────────────────── */

static esp_err_t assistant_set(const cJSON *v)
{
    if (!cJSON_IsObject(v)) {
        return ESP_ERR_INVALID_ARG;
    }
    /* 현재 설정에 요청 값을 덮어쓴 뒤 assistant_save가 전체를 검증한다 (모르는 키가 있으면 키 개수로 거부) */
    cJSON *merged = assistant_settings();
    if (!merged) {
        return ESP_ERR_NO_MEM;
    }
    const cJSON *item;
    cJSON_ArrayForEach(item, v) {
        cJSON *copy = cJSON_Duplicate(item, true);
        if (!copy) {
            cJSON_Delete(merged);
            return ESP_ERR_NO_MEM;
        }
        if (cJSON_HasObjectItem(merged, item->string)) {
            cJSON_ReplaceItemInObjectCaseSensitive(merged, item->string, copy);
        } else {
            cJSON_AddItemToObject(merged, item->string, copy);
        }
    }
    /*
     * 키 검사 (docs/design-voice-saver-mode.md 11.1): ai_provider를 "이번 요청에서" 키가 없는 회사로 바꾸려
     * 하면 거절. 앱이 화면에서 막지만 스피커도 한 번 더 막는다. LAN 409 NO_API_KEY, BLE
     * {"ok":false,"error":"NO_API_KEY"} (local_api.c put_settings·ble_cmd.c err_code가
     * ESP_ERR_NOT_ALLOWED를 NO_API_KEY로 바꾼다). ai_provider를 건드리지 않는 요청(이름 바꾸기 등)은
     * 나중에 그 키가 지워졌더라도 여기서 막지 않는다 — 그건 대화 시작 때(live_voice_start) 막는다.
     */
    if (cJSON_HasObjectItem(v, "ai_provider")) {
        const cJSON *provider = cJSON_GetObjectItemCaseSensitive(merged, "ai_provider");
        bool gemini = cJSON_IsString(provider) && !strcmp(provider->valuestring, "gemini");
        if (!secret_exists(gemini ? SECRET_GEMINI_KEY : SECRET_OPENAI_KEY)) {
            cJSON_Delete(merged);
            return ESP_ERR_NOT_ALLOWED;
        }
    }
    esp_err_t err = assistant_save(merged);
    cJSON_Delete(merged);
    if (err == ESP_OK) {
        notify(APP_SETTING_ASSISTANT);
    }
    return err;
}

/* ── led ──────────────────────────────────────────────────── */

static const char *const LED_KEYS[] = {"brightness"};

static cJSON *led_get(void)
{
    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "brightness", settings_get_led_brightness());
    return j;
}

static esp_err_t led_set(const cJSON *v)
{
    int brightness = settings_get_led_brightness();
    if (!only_keys(v, LED_KEYS, 1) || !take_int(v, "brightness", 0, 100, &brightness)) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = settings_set_led_brightness(brightness);
    if (err == ESP_OK) {
        led_ring_set_brightness(brightness);
        notify(APP_SETTING_LED);
    }
    return err;
}

/* ── 공개 API ─────────────────────────────────────────────── */

cJSON *device_settings_get(const char *group)
{
    if (!group) {
        return NULL;
    }
    if (!strcmp(group, "audio")) {
        return audio_get();
    }
    if (!strcmp(group, "wake")) {
        return wake_get();
    }
    if (!strcmp(group, "assistant")) {
        return assistant_settings();
    }
    if (!strcmp(group, "led")) {
        return led_get();
    }
    return NULL;
}

esp_err_t device_settings_set(const char *group, const cJSON *value)
{
    if (!group) {
        return ESP_ERR_NOT_FOUND;
    }
    if (!strcmp(group, "audio")) {
        return audio_set(value);
    }
    if (!strcmp(group, "wake")) {
        return wake_set(value);
    }
    if (!strcmp(group, "assistant")) {
        return assistant_set(value);
    }
    if (!strcmp(group, "led")) {
        return led_set(value);
    }
    return ESP_ERR_NOT_FOUND;
}
