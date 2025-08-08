idf_component_register(SRCS "tcp_server.c" INCLUDE_DIRS "."
                        REQUIRES command_bus ota_handler)