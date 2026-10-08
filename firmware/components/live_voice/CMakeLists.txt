idf_component_register(SRCS "live_voice.c" "delegation.c" "hangup_phrase.c" "saver_voice.c" "saver_endpoint.c"
                             "saver_text.c" "saver_resample.c" "gemini_schema.c" "gemini_base64.c"
                             "gemini_live.c" "gemini_live_proto.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "."
                       REQUIRES app_config app_events settings audio_core wake net json mbedtls esp_timer
                                esp_wifi tcp_transport lwip freertos heap espressif__esp_websocket_client
                                esp_http_client esp-tls espressif__esp-sr)
