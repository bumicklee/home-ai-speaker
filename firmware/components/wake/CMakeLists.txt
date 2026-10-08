idf_component_register(SRCS "wake.c"
                       INCLUDE_DIRS "include"
                       REQUIRES app_config app_events settings audio_core esp_timer freertos heap
                                espressif__esp-sr)
