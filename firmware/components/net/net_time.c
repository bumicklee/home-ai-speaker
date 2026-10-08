/*
 * SNTP 시간 동기화. TLS 인증서 검증에 정확한 시각이 필요하다.
 * 부팅 때는 RTC(PCF85063) 시각을 먼저 쓰고(main이 복원), SNTP가 맞추면 콜백으로 RTC를 갱신한다.
 */
#include "net.h"

#include <time.h>
#include "app_config.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"

static const char *TAG = "SNTP";

static net_time_sync_cb_t s_cb;

bool net_time_valid(void)
{
    return time(NULL) >= APP_TIME_VALID_EPOCH;
}

void net_set_time_sync_cb(net_time_sync_cb_t cb)
{
    s_cb = cb;
}

static void on_sync(struct timeval *tv)
{
    ESP_LOGI(TAG, "time synchronized");
    if (s_cb) {
        s_cb();
    }
}

esp_err_t net_time_start(void)
{
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG(APP_SNTP_SERVER);
    cfg.sync_cb = on_sync;
    return esp_netif_sntp_init(&cfg);
}
