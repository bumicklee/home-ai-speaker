idf_component_register(SRCS "audio_core.c" "startup_sound.c"
                       INCLUDE_DIRS "include"
                       REQUIRES app_config app_events board settings esp_driver_i2s esp_timer freertos
                                heap espressif__esp-sr espressif__esp_audio_codec
                       EMBED_FILES "assets/startup.mp3" "assets/ready.mp3")
