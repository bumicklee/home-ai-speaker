idf_component_register(SRCS "telegram.c" "tg_store.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "."
                       REQUIRES app_config app_events settings net json esp_http_client esp-tls esp_timer
                                esp_hw_support freertos heap nvs_flash)
