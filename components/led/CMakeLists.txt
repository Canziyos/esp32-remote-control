idf_component_register(SRCS "led.c" 
                        PRIV_REQUIRES command_bus
                        INCLUDE_DIRS "."
                        REQUIRES esp_driver_gpio)