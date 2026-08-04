idf_component_register(SRCS "menu.c" "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver esp_adc led_strip GP1247AI)