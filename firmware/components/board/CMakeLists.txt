idf_component_register(SRCS "board.c" "codec.c" "led_ring.c" "buttons.c" "rtc.c"
                       INCLUDE_DIRS "include"
                       REQUIRES app_config app_events esp_driver_i2c esp_driver_i2s esp_driver_gpio
                                esp_timer freertos espressif__esp_io_expander_tca95xx_16bit
                                espressif__esp_codec_dev espressif__led_strip)
