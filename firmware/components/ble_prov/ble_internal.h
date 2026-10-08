/*
 * ble_prov 내부 공유 상태. ble_prov.c(창·작업 태스크), ble_gatt.c(GATT·GAP),
 * ble_cmd.c(명령 프로토콜)가 같이 쓴다.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/* 작업 태스크 일감. GATT 콜백은 오래 걸리는 일을 직접 하지 않고 여기에 넣는다 (2.2절) */
typedef enum {
    JOB_OPEN,
    JOB_CLOSE,
    JOB_TICK,
    JOB_CONNECTED,
    JOB_DISCONNECTED,
    JOB_SCAN,
    JOB_PASSWORD,           /* 비밀번호가 써짐 → 시험 연결 */
    JOB_WIFI_RESULT,        /* arg = net_wifi_status_t */
    JOB_PROV_DONE,          /* 0xFF88 */
    JOB_COMMAND,            /* data = 재조립된 JSON (작업 태스크가 free) */
    JOB_COMMAND_BUSY,       /* arg = 요청 id */
    JOB_COMMAND_ERROR,      /* arg = 오류 종류, 재조립 실패 */
} ble_job_type_t;

typedef struct {
    ble_job_type_t type;
    int arg;
    char *data;
    size_t len;
} ble_job_t;

bool ble_post_job(ble_job_type_t type, int arg, char *data, size_t len);

/* ── 연결 상태 (ble_gatt.c가 갱신) ───────────────────────── */
typedef struct {
    volatile bool connected;
    volatile uint16_t conn_handle;
    volatile bool cmd_tx_subscribed;
    uint16_t status_handle, scan_handle, cmd_tx_handle;
    /* 이번 연결에서 받은 Wi-Fi 정보 (저장 전) */
    char ssid[33];
    char password[65];
    volatile uint8_t wifi_status;       /* 0xFF53 값 */
} ble_state_t;

extern ble_state_t g_ble;

/* ble_gatt.c */
esp_err_t ble_stack_start(const char *device_name);
void      ble_stack_stop(void);
/** 알림 전송. 버퍼가 없으면 20ms 뒤 최대 10회 재시도 (작업 태스크에서만 부른다) */
esp_err_t ble_notify(uint16_t handle, const void *data, size_t len);
uint16_t  ble_mtu(void);

/* ble_cmd.c */
/** 0xFF81 쓰기 조각 하나 (GATT 콜백에서 불림, 막지 않음). ATT 오류 코드 또는 0 */
int  ble_cmd_on_fragment(const uint8_t *data, size_t len);
void ble_cmd_reset(void);
/** 작업 태스크: 명령 처리 후 응답 */
void ble_cmd_handle(char *json, size_t len);
void ble_cmd_reply_error(int id, const char *code);
/** prov.close 요청이 왔는가 (응답을 보낸 뒤 창을 닫으려고) */
bool ble_cmd_take_close_request(void);
void ble_cmd_frame_error(int kind);
