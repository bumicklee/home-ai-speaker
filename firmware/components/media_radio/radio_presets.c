/*
 * 라디오 프리셋: storage(littlefs) 파티션의 /storage/presets.json (설계서 2.9절, RAD-08·12).
 * URL이 길어 NVS 문자열 한도(4,000B)를 넘을 수 있어 파일로 둔다.
 *
 * 기본 프리셋 13개 (RAD-12, 2026-10-03 사용자 요청: 한국 채널 + CNN·블룸버그 + 미국·한국 토크).
 * 모두 MP3/AAC 직접 스트림이다 (HLS는 이 재생기로 못 튼다). 2026-10-03 Mac에서 접속·형식 확인.
 * - KBS·MBC·SBS·CBS·YTN·TBS·EBS·경인방송·TBN·아리랑 등 한국 주요 방송은 전부 HLS(대부분 토큰)라 넣지 못했다.
 *   한국 채널은 인터넷 방송(K-POP, 올드팝)과 공동체 라디오(마포FM, 전주FM, 말 위주)다.
 * - CNN·CNN International·Bloomberg는 TuneIn CDN 주소다. 방송사 공식 주소가 아니라 바뀌거나 막힐 수 있다.
 * - 이름에 한글·영어를 함께 넣어 "블룸버그 틀어 줘"도 이름 일부로 찾게 했다 (radio_presets_find는 부분 일치).
 * 예전 기본값 6개를 그대로 쓰던 기기는 부팅 때 새 기본값으로 바꾼다 (사용자가 고친 목록은 건드리지 않음).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "app_config.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "media_radio.h"

static const char *TAG = "PRESETS";

#define BASE_PATH  "/storage"
#define FILE_PATH  BASE_PATH "/presets.json"
#define FILE_MAX   (RADIO_MAX_PRESETS * (RADIO_NAME_MAX + RADIO_URL_MAX + RADIO_GENRE_MAX + 40))

static const char DEFAULTS[] =
    "["
    "{\"name\":\"Big B Radio K-POP 케이팝\",\"url\":\"https://antares.dribbcast.com/proxy/kpop?mp=/s\",\"genre\":\"kpop,pop,korean\"},"
    "{\"name\":\"ARA FM 아라FM\",\"url\":\"http://141.164.38.238/listen/ara_fm/radio.mp3\",\"genre\":\"kpop,ballad,chill,korean\"},"
    "{\"name\":\"올드팝카페 Oldpop Cafe\",\"url\":\"http://cast.oldpopcafe.net:7080/\",\"genre\":\"oldies,pop,newage,korean\"},"
    "{\"name\":\"LISTEN.moe K-POP\",\"url\":\"https://listen.moe/kpop/fallback\",\"genre\":\"kpop,pop\"},"
    "{\"name\":\"마포FM 공동체라디오\",\"url\":\"http://onair.mapofm.net/mapofm\",\"genre\":\"talk,community,korean\"},"
    "{\"name\":\"전주FM 공동체라디오\",\"url\":\"http://radiostream.communityradio.kr/jcfm1\",\"genre\":\"talk,community,korean\"},"
    "{\"name\":\"CNN\",\"url\":\"https://tunein.cdnstream1.com/2868_96.mp3\",\"genre\":\"news,talk,english\"},"
    "{\"name\":\"CNN International\",\"url\":\"https://tunein.cdnstream1.com/3519_96.aac\",\"genre\":\"news,world,english\"},"
    "{\"name\":\"블룸버그 Bloomberg Radio\",\"url\":\"https://tunein.cdnstream1.com/3521_96.mp3\",\"genre\":\"business,finance,news,english\"},"
    "{\"name\":\"NPR 미국 공영라디오\",\"url\":\"http://npr-ice.streamguys1.com/live.mp3\",\"genre\":\"talk,news,english\"},"
    "{\"name\":\"WNYC 뉴욕 공영라디오\",\"url\":\"https://fm939.wnyc.org/wnycfm-web\",\"genre\":\"talk,news,english\"},"
    "{\"name\":\"C-SPAN Radio\",\"url\":\"https://playerservices.streamtheworld.com/api/livestream-redirect/CSPANRADIO.mp3\",\"genre\":\"politics,talk,english\"},"
    "{\"name\":\"BBC World Service\",\"url\":\"https://stream.live.vc.bbcmedia.co.uk/bbc_world_service\",\"genre\":\"news,world,english\"}"
    "]";

/* 예전(2026-10-02) 기본값. 이것과 똑같이 저장된 기기만 새 기본값으로 바꾼다 */
static const char OLD_DEFAULTS_V1[] =
    "["
    "{\"name\":\"Radio Paradise Mellow\",\"url\":\"http://stream.radioparadise.com/mellow-128\",\"genre\":\"chill,mellow,eclectic\"},"
    "{\"name\":\"Radio Paradise Rock\",\"url\":\"http://stream.radioparadise.com/rock-128\",\"genre\":\"rock\"},"
    "{\"name\":\"SomaFM Groove Salad\",\"url\":\"https://ice1.somafm.com/groovesalad-128-mp3\",\"genre\":\"ambient,chill,electronic\"},"
    "{\"name\":\"SomaFM DEF CON Radio\",\"url\":\"https://ice1.somafm.com/defcon-128-mp3\",\"genre\":\"electronic\"},"
    "{\"name\":\"SomaFM Indie Pop Rocks\",\"url\":\"https://ice1.somafm.com/indiepop-128-mp3\",\"genre\":\"indie,pop\"},"
    "{\"name\":\"KEXP\",\"url\":\"https://kexp-mp3-128.streamguys1.com/kexp128.mp3\",\"genre\":\"alternative,indie,eclectic\"}"
    "]";

static SemaphoreHandle_t s_lock;
static cJSON *s_presets;
static bool s_mounted;

static bool string_ok(const cJSON *v, size_t min, size_t max)
{
    return cJSON_IsString(v) && strlen(v->valuestring) >= min && strlen(v->valuestring) <= max;
}

static bool presets_valid(const cJSON *arr)
{
    if (!cJSON_IsArray(arr) || cJSON_GetArraySize(arr) > RADIO_MAX_PRESETS) {
        return false;
    }
    const cJSON *p;
    cJSON_ArrayForEach(p, arr) {
        const cJSON *url = cJSON_GetObjectItemCaseSensitive(p, "url");
        const cJSON *genre = cJSON_GetObjectItemCaseSensitive(p, "genre");
        if (!cJSON_IsObject(p) || !string_ok(cJSON_GetObjectItemCaseSensitive(p, "name"), 1, RADIO_NAME_MAX) ||
            !string_ok(url, 8, RADIO_URL_MAX) ||
            (strncmp(url->valuestring, "http://", 7) && strncmp(url->valuestring, "https://", 8)) ||
            (genre && !string_ok(genre, 0, RADIO_GENRE_MAX))) {
            return false;
        }
    }
    return true;
}

static esp_err_t write_file(const cJSON *arr)
{
    if (!s_mounted) {
        return ESP_ERR_INVALID_STATE;
    }
    char *text = cJSON_PrintUnformatted(arr);
    if (!text) {
        return ESP_ERR_NO_MEM;
    }
    FILE *f = fopen(FILE_PATH ".tmp", "w");
    esp_err_t err = ESP_FAIL;
    if (f) {
        bool ok = fputs(text, f) >= 0;
        ok = (fclose(f) == 0) && ok;
        /* 임시 파일에 다 쓴 뒤 바꿔 끼운다 (쓰다가 전원이 꺼져도 이전 목록이 남게) */
        if (ok) {
            remove(FILE_PATH);
            ok = rename(FILE_PATH ".tmp", FILE_PATH) == 0;
        }
        err = ok ? ESP_OK : ESP_FAIL;
    }
    free(text);
    return err;
}

static cJSON *read_file(void)
{
    FILE *f = s_mounted ? fopen(FILE_PATH, "r") : NULL;
    if (!f) {
        return NULL;
    }
    char *buf = malloc(FILE_MAX + 1);
    cJSON *j = NULL;
    if (buf) {
        size_t n = fread(buf, 1, FILE_MAX, f);
        buf[n] = '\0';
        j = cJSON_Parse(buf);
        free(buf);
    }
    fclose(f);
    return j;
}

esp_err_t radio_presets_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        return ESP_ERR_NO_MEM;
    }
    const esp_vfs_littlefs_conf_t conf = {
        .base_path = BASE_PATH,
        .partition_label = APP_PART_STORAGE,
        .format_if_mount_failed = true,
    };
    esp_err_t err = esp_vfs_littlefs_register(&conf);
    s_mounted = err == ESP_OK;
    if (!s_mounted) {
        ESP_LOGE(TAG, "littlefs mount failed: %s (using defaults in RAM)", esp_err_to_name(err));
    }
    s_presets = read_file();
    if (!presets_valid(s_presets)) {
        cJSON_Delete(s_presets);
        s_presets = cJSON_Parse(DEFAULTS);
        write_file(s_presets);
    } else {
        cJSON *old = cJSON_Parse(OLD_DEFAULTS_V1);
        if (old && cJSON_Compare(s_presets, old, true)) {
            ESP_LOGI(TAG, "old default presets: replacing with new defaults");
            cJSON_Delete(s_presets);
            s_presets = cJSON_Parse(DEFAULTS);
            write_file(s_presets);
        }
        cJSON_Delete(old);
    }
    ESP_LOGI(TAG, "%d preset(s)", cJSON_GetArraySize(s_presets));
    return s_presets ? ESP_OK : ESP_ERR_NO_MEM;
}

cJSON *radio_presets_get(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    cJSON *copy = cJSON_Duplicate(s_presets, true);
    xSemaphoreGive(s_lock);
    return copy;
}

esp_err_t radio_presets_set(const cJSON *presets)
{
    if (!presets_valid(presets)) {
        return ESP_ERR_INVALID_ARG;
    }
    cJSON *copy = cJSON_Duplicate(presets, true);
    if (!copy) {
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t err = write_file(copy);
    if (err == ESP_OK) {
        cJSON_Delete(s_presets);
        s_presets = copy;
        copy = NULL;
    }
    xSemaphoreGive(s_lock);
    cJSON_Delete(copy);
    return err;
}

esp_err_t radio_presets_reset(void)
{
    cJSON *def = cJSON_Parse(DEFAULTS);
    esp_err_t err = radio_presets_set(def);
    cJSON_Delete(def);
    return err;
}

int radio_presets_count(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    int n = cJSON_GetArraySize(s_presets);
    xSemaphoreGive(s_lock);
    return n;
}

bool radio_presets_entry(int index, char *url, size_t url_len, char *name, size_t name_len)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    const cJSON *p = cJSON_GetArrayItem(s_presets, index);
    bool ok = p != NULL;
    if (ok) {
        strlcpy(url, cJSON_GetObjectItem(p, "url")->valuestring, url_len);
        strlcpy(name, cJSON_GetObjectItem(p, "name")->valuestring, name_len);
    }
    xSemaphoreGive(s_lock);
    return ok;
}

/* 대소문자를 무시하고 부분 문자열 찾기 (한글은 바이트 그대로 비교) */
static bool contains_ci(const char *hay, const char *needle)
{
    size_t n = strlen(needle);
    if (!n) {
        return false;
    }
    for (; *hay; hay++) {
        if (!strncasecmp(hay, needle, n)) {
            return true;
        }
    }
    return false;
}

int radio_presets_find(const char *name, const char *genre)
{
    int found = -1;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    int i = 0;
    const cJSON *p;
    cJSON_ArrayForEach(p, s_presets) {
        const cJSON *pn = cJSON_GetObjectItem(p, "name"), *pg = cJSON_GetObjectItem(p, "genre");
        if ((name && name[0] && contains_ci(pn->valuestring, name)) ||
            (genre && genre[0] && cJSON_IsString(pg) && contains_ci(pg->valuestring, genre))) {
            found = i;
            break;
        }
        i++;
    }
    xSemaphoreGive(s_lock);
    return found;
}
