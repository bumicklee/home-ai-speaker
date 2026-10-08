/**
 * @file media_radio.h
 * @brief 인터넷 라디오 (설계서 2.9절, 기능 RAD-01–12)
 *
 * HTTP/HTTPS 스트림을 esp_http_client로 직접 받는다: 302 리다이렉트, .pls/.m3u 재생목록,
 * ICY 메타데이터(곡 제목), MP3·AAC(HE-AAC 포함). 해독한 소리를 모노·출력 레이트로 바꿔
 * audio_core 음악 채널에 넣는다. 끊기면 1초부터 최대 30초 간격으로 5회 재연결.
 *
 * 프리셋(최대 20개)은 storage(littlefs) 파티션의 /storage/presets.json.
 * 상태가 바뀌면 APP_EVT_MEDIA_STATE(app_media_event_t)를 보낸다.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "app_events.h"
#include "cJSON.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RADIO_MAX_PRESETS 20
#define RADIO_NAME_MAX    48
#define RADIO_URL_MAX     256
#define RADIO_SEARCH_MAX  3     /* 검색 후보 수. 첫 후보가 HLS·연결 실패면 다음 후보로 */

/** 방송 하나 (검색 결과) */
typedef struct {
    char url[RADIO_URL_MAX + 1];
    char name[RADIO_NAME_MAX + 1];
} radio_station_t;
#define RADIO_GENRE_MAX   64

typedef struct {
    media_play_state_t state;
    int preset;                         /* 프리셋으로 재생 중이면 번호, 아니면 -1 */
    char station[RADIO_NAME_MAX + 1];
    char icy_title[128];                /* 지금 나오는 곡 (방송이 알려줄 때만) */
    unsigned bitrate_kbps;
} radio_status_t;

/** littlefs를 붙이고 프리셋을 읽고 재생 태스크를 시작한다 */
esp_err_t media_radio_init(void);

esp_err_t radio_play_url(const char *url, const char *title);
/** list[0]을 틀고, 틀 수 없으면(HLS·연결 실패) 나머지를 차례로 시도한다. count 1..RADIO_SEARCH_MAX */
esp_err_t radio_play_stations(const radio_station_t *list, int count);
esp_err_t radio_play_preset(int index);
/** 마지막으로 들은 방송 (없으면 첫 프리셋). "라디오 틀어 줘" */
esp_err_t radio_play_last(void);
esp_err_t radio_stop(void);
esp_err_t radio_pause(void);
esp_err_t radio_resume(void);
/** 프리셋 앞뒤로 (+1 다음, -1 이전) */
esp_err_t radio_step(int delta);
bool      radio_is_active(void);        /* 버퍼링·재생·일시정지 중 */

void   radio_get_status(radio_status_t *out);
/** /api/v1/media/now 형식: source, state, title, artist, station */
cJSON *radio_now_json(void);

/* ── 프리셋 (radio_presets.c) ────────────────────────────── */

/** [{name, url, genre}] 복사본 */
cJSON    *radio_presets_get(void);
/** 전체 교체. 검증에 실패하면 ESP_ERR_INVALID_ARG */
esp_err_t radio_presets_set(const cJSON *presets);
/** 기본 프리셋으로 되돌린다 (공장 초기화) */
esp_err_t radio_presets_reset(void);
/** 이름 일부(대소문자 무시) 또는 장르 태그로 찾는다. 없으면 -1 */
int       radio_presets_find(const char *name, const char *genre);
int       radio_presets_count(void);
/** index번 프리셋의 url·name을 복사한다 */
bool      radio_presets_entry(int index, char *url, size_t url_len, char *name, size_t name_len);

/* ── 방송 검색 (radio_search.c, Radio-Browser) ───────────── */

/**
 * 장르 태그(by_tag) 또는 이름으로 찾아 인기 순으로 재생 가능해 보이는 방송을 최대 max개 고른다.
 * *found에 개수. 하나도 없으면 ESP_ERR_NOT_FOUND. 막는 함수(HTTPS, 수 초). 툴 워커에서 부른다.
 */
esp_err_t radio_search(const char *query, bool by_tag, radio_station_t *out, int max, int *found);

#ifdef __cplusplus
}
#endif
