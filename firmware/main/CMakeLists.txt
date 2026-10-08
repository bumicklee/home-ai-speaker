idf_component_register(SRCS "main.c" "dev_console.c"
                       INCLUDE_DIRS "."
                       REQUIRES app_config app_events settings board audio_core wake live_voice
                                assistant media_radio media_spotify messenger_telegram notes net
                                ble_prov local_api device_api esp_system esp_app_format app_update console
                                esp_driver_usb_serial_jtag heap json)
