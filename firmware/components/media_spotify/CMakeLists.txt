# CONFIG_APP_SPOTIFY(main/Kconfig.projbuild, 기본 끔)가 꺼져 있으면 실제 코드 대신 대체 코드를 빌드한다.
# 공개 API(media_spotify.h)는 같으므로 부르는 쪽은 바뀌지 않는다.
if(CONFIG_APP_SPOTIFY)
    set(srcs "spotify_api.c")
else()
    set(srcs "spotify_off.c")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       REQUIRES app_config app_events settings net json esp_http_client esp-tls esp_timer
                                freertos heap)
