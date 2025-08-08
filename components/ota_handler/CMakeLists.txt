idf_component_register(SRCS "ota_handler.c" INCLUDE_DIRS "."
                        PRIV_REQUIRES command_bus app_update)