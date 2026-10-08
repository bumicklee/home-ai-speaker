idf_component_register(SRCS "assistant.c" "tool_worker.c" "mic_control.c" "tool_guard.c" "lesson.c"
                       INCLUDE_DIRS "include"
                       REQUIRES app_config app_events settings audio_core wake live_voice media_radio messenger_telegram media_spotify notes json esp_hw_support freertos esp_timer esp_http_client mbedtls nvs_flash
                                espressif__esp-sr)
