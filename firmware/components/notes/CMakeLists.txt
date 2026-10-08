idf_component_register(SRCS "notes.c" "notes_logic.c"
                       INCLUDE_DIRS "include"
                       REQUIRES app_config app_events settings json freertos esp_timer nvs_flash esp_hw_support heap)
