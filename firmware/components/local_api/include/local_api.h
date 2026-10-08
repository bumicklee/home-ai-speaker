/**
 * @file local_api.h
 * @brief 같은 Wi-Fi의 앱용 HTTP REST + WebSocket 이벤트 (설계서 2.14절)
 *
 * 모든 요청은 Authorization: Bearer <api.pair 토큰>. 비밀값 쓰기는 LAN에서 받지 않는다(D6).
 * 라디오·Telegram·Spotify·OTA 엔드포인트는 해당 Phase에서 추가한다.
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 포트 80에서 서버를 시작하고 이벤트 중계를 구독한다 */
esp_err_t local_api_start(void);

#ifdef __cplusplus
}
#endif
