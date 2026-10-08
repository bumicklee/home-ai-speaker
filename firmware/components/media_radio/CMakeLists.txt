idf_component_register(SRCS "radio_player.c" "radio_presets.c" "radio_search.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "."
                       REQUIRES app_config app_events settings audio_core net json esp_http_client esp-tls
                                esp_timer freertos heap espressif__esp_audio_codec espressif__esp_audio_effects
                                joltwallet__littlefs vfs nvs_flash)
