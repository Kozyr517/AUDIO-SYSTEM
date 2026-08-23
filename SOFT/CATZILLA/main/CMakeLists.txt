idf_component_register(
    SRCS 
        "lcd.c" 
        "main.c"
    INCLUDE_DIRS 
        "."
    REQUIRES 
        DS18b20 driver esp_adc led_strip GP1247AI APP_UI
)