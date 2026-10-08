idf_component_register(SRCS "settings.c" "secret_store.c"
                       INCLUDE_DIRS "include"
                       REQUIRES nvs_flash app_config)
