idf_component_register(SRCS "ble_prov.c" "ble_gatt.c" "ble_cmd.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "."
                       REQUIRES app_config app_events settings net device_api media_spotify bt json esp_timer esp_system
                                esp_wifi freertos heap)
