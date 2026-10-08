idf_component_register(SRCS "local_api.c" "api_auth.c" "api_events.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "."
                       REQUIRES app_config app_events settings audio_core wake live_voice assistant media_radio messenger_telegram media_spotify net device_api
                                notes esp_http_server json lwip esp_timer esp_system freertos)
