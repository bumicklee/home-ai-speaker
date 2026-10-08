/* local_api 내부 공유 */
#pragma once

#include <stdbool.h>
#include "cJSON.h"
#include "device_api.h"
#include "esp_http_server.h"

/**
 * 요청을 검사한다: Host(DNS 리바인딩), Origin(브라우저), 실패 횟수 제한, Bearer 토큰.
 * 통과하지 못하면 오류 응답을 이미 보낸 상태로 false. phone_id는 NULL이어도 된다.
 */
bool api_authorize(httpd_req_t *req, char phone_id[PHONE_ID_MAX + 1]);

/* 응답 (cJSON은 소유권을 넘긴다) */
esp_err_t api_send_json(httpd_req_t *req, const char *status, cJSON *body);
esp_err_t api_send_error(httpd_req_t *req, const char *status, const char *code);
esp_err_t api_send_ok(httpd_req_t *req);

/* WebSocket 이벤트 (api_events.c) */
esp_err_t api_events_register(httpd_handle_t server);
void      api_events_on_close(int sockfd);
