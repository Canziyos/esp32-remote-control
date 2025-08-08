idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES spi_flash command_bus led tcp_server ota_handler
                       INCLUDE_DIRS "."
                       REQUIRES esp_wifi esp_event nvs_flash)