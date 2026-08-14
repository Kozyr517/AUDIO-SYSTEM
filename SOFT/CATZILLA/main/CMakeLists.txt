idf_component_register(SRCS "main.c" "menu.c"
                       INCLUDE_DIRS "."
                       REQUIRES ds18b20 driver esp_adc led_strip GP1247AI)