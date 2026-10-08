/**
 * @file media_spotify.h
 * @brief Spotify Web API (설계서 2.10절, 기능 SPT-*)
 *
 * 이 파일은 Web API 쪽(계정 연결·토큰 갱신·검색·재생 제어·지금 재생 중)이다.
 * 스피커가 직접 소리를 내는 Connect 수신기(cspot 계열)는 Q2 결정 전이라 아직 없다.
 * 그동안 재생 대상은 같은 계정의 Spotify 기기 중에서 고른다(이 스피커 이름 → 활성 기기 → 첫 기기).
 *
 * 2026년 Spotify Development Mode 변경을 반영했다:
 * - 검색 limit 최대 10, GET /me에 product(Premium 여부)가 없다 → 재생 명령의 403으로 Premium을 판단
 * - 가수 인기곡(top-tracks)이 없어졌다 → 가수는 artist 컨텍스트로 재생
 * - refresh token은 처음 인증에서 6개월 뒤 만료되고 갱신해도 늘지 않는다 → 400이면 다시 로그인 안내
 *
 * 비밀값(client_id, refresh token, access token)은 로그·응답에 넣지 않는다.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "cJSON.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SPOTIFY_OFF,                /* 연결 안 됨 */
    SPOTIFY_OK,
    SPOTIFY_RELOGIN_REQUIRED,   /* refresh token 만료·해지 (6개월) */
    SPOTIFY_PREMIUM_REQUIRED,   /* 재생 제어는 Premium만 */
    SPOTIFY_ERROR,              /* 네트워크 등 일시 오류 */
} spotify_state_t;

esp_err_t media_spotify_init(void);

/**
 * 앱이 PKCE로 받은 client_id(32자리 16진수)와 refresh token을 저장한다.
 * 저장하기 전에 토큰 갱신을 한 번 해 보고, 실패하면 저장하지 않고 ESP_ERR_INVALID_ARG.
 * 막는 함수(HTTPS). BLE 작업 태스크에서 부른다.
 */
esp_err_t spotify_link(const char *client_id, const char *refresh_token);
/** client_id·refresh token·Connect 자격 정보를 지운다 */
esp_err_t spotify_logout(void);
bool      spotify_linked(void);

spotify_state_t spotify_state(void);
const char     *spotify_state_name(spotify_state_t state);

/**
 * 검색해서 재생한다. type: track · artist · album · playlist.
 * desc에 "제목 - 가수" 같은 설명을 채운다. 막는 함수(HTTPS). 툴 워커에서 부른다.
 * ESP_ERR_NOT_FOUND: 결과 없음, ESP_ERR_INVALID_STATE: 재생할 기기 없음, ESP_ERR_NOT_ALLOWED: Premium 아님
 */
esp_err_t spotify_play(const char *query, const char *type, char *desc, size_t desc_len);

/** pause · resume · next · previous · stop(= pause). 막는 함수 */
esp_err_t spotify_control(const char *action);

/** /api/v1/media/now 형식 (재생 중이 아니면 source none). 막는 함수 */
cJSON *spotify_now_json(void);
/** GET /api/v1/spotify: linked, state, account, device, last_error */
cJSON *spotify_status_json(void);

/** 마지막 재생 상태를 캐시에서 (막지 않음). 툴·상태 표시용 */
bool spotify_recently_playing(void);
/** 마지막으로 받은 now 결과 복사본 (막지 않음). LAN 핸들러용. 아직 없으면 source none */
cJSON *spotify_now_cached(void);

#ifdef __cplusplus
}
#endif
