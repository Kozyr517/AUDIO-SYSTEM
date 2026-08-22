idf_component_register(
    SRCS 
        "ui_app.c"
        "screens.c"
        "menu.c"
    INCLUDE_DIRS 
        "."
    REQUIRES 
        GP1247AI 
        ADAU 
        analizator
        ANIMATIONS 
        driver
        led_strip

)