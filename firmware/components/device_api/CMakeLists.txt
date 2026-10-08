idf_component_register(SRCS "device_settings.c" "device_info.c" "phones.c"
                       INCLUDE_DIRS "include"
                       REQUIRES app_config app_events settings board audio_core wake live_voice assistant media_radio messenger_telegram media_spotify net
                                json mbedtls esp_app_format esp_wifi esp_system esp_timer esp_hw_support freertos heap)
