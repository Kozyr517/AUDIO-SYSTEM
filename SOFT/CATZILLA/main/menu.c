#include <stdio.h>
#include "menu.h"
#include "lcd.h"
#include "gp1247ai.h"
#include "Font16x16.h"
#include "Sinclair_S8x8.h"
#include "animation.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "MENU_SETTINGS";

// Глобальний об'єкт дисплея з lcd.c
extern TypeDef_GP1247AI lcd;

// Зовнішні змінні з buttons.c
extern volatile bool is_muted;
extern float myspace_distances[7];
extern bool is_myspace_editing;

uint8_t menu_pointer = 0;
uint8_t main_menu_pointer = 0;
uint8_t filters_menu_pointer = 0;
uint8_t eq_menu_pointer = 0;
uint8_t myspace_menu_pointer = 0;
uint8_t phono_menu_pointer = 0;

uint8_t old_menu_pointer = 255;
uint8_t old_main_menu_pointer = 0;
uint8_t old_filters_menu_pointer = 0;
uint8_t old_eq_menu_pointer = 0;
uint8_t old_myspace_menu_pointer = 0;
uint8_t old_phono_menu_pointer = 0;

static bool old_is_muted = false;
static bool old_is_myspace_editing = false;

// ==================== ЗБЕРЕЖЕНІ НАЛАШТУВАННЯ В RAM ====================
uint8_t saved_eq_preset  = 0;
uint8_t saved_filter     = 0;
uint8_t saved_myspace    = 0;
uint8_t saved_spatial    = 0;
uint8_t saved_sys        = 0;

const char* eq_names[6]      = {"POP", "ROCK", "JAZZ", "SYMPH", "NATURE", "B&T"};
const char* filter_names[6]  = {"SILK", "PURRITY", "DRIVE", "ATMOS", "VILVET", "DIRECT"};
const char* spatial_names[2] = {"SURROUND", "MUSIC"};
const char* sys_names[2]     = {"SYS 2.1", "SYS 5.1"};

// 7 пунктів для MySpace
const char* myspace_names[7] = {
    "ROOM DEPTH", 
    "FRONT L", 
    "FRONT R", 
    "CENTER", 
    "SUBWOOFER", 
    "REAR L", 
    "REAR R"
};

// Малювання заповненого квадрата/кубика 4x4 пікселі у верхньому правому кутку комірки
static void draw_active_square(int x, int y) {
    for (int dy = 0; dy < 4; dy++) {
        for (int dx = 0; dx < 4; dx++) {
            LCD_DrawPixel(&lcd, x + dx, y + dy, 1);
        }
    }
}

// Малювання плашки MUTE у правому верхньому кутку вікна анімації/інформації
static void draw_mute_badge(int x, int y) {
    if (is_muted) {
        // Зовнішній рамковий прямокутник
        lcd_draw_rectangle(x, y, 42, 12);
        // Заповнений індикатор під текст MUTE
        lcd_print("MUTE", x + 5, y + 2, (const uint8_t*)Sinclair_S8x8, 0);
    }
}


static const uint8_t eq_pop[10]    = {29, 40, 44, 45, 41, 30, 28, 28, 29, 29};
static const uint8_t eq_rock[10]   = {45, 40, 23, 19, 26, 39, 47, 50, 50, 50};
static const uint8_t eq_jazz[10]   = {40, 38, 32, 33, 31, 35, 39, 41, 43, 44};
static const uint8_t eq_symph[10]  = {49, 49, 42, 42, 33, 24, 24, 24, 33, 33};
static const uint8_t eq_nature[10] = {33, 33, 33, 33, 33, 33, 33, 33, 33, 33};
static const uint8_t eq_bnt[10]    = {48, 48, 48, 35, 25, 20, 25, 30, 35, 40};

static const uint8_t* eq_presets[6] = {eq_pop, eq_rock, eq_jazz, eq_symph, eq_nature, eq_bnt};

static void set_main_menu(void) {
    lcd_clear();
    switch (main_menu_pointer) {
        case 0: lcd_draw_rectangle(1, 1, 92, 20); break;
        case 1: lcd_draw_rectangle(1, 22, 92, 20); break;
        case 2: lcd_draw_rectangle(1, 43, 92, 20); break;
        case 3: lcd_draw_rectangle(95, 1, 92, 20); break;
        case 4: lcd_draw_rectangle(95, 22, 92, 20); break;
        case 5: lcd_draw_rectangle(95, 43, 92, 20); break;
    }
    lcd_draw_rectangle(2, 2, 90, 18);
    lcd_print("EQ PRESET", 11, 7, (const uint8_t*)Sinclair_S8x8, 0);
    lcd_draw_rectangle(2, 23, 90, 18);
    lcd_print("FILTERS", 19, 28, (const uint8_t*)Sinclair_S8x8, 0);
    lcd_draw_rectangle(2, 44, 90, 18);
    lcd_print("MY SPACE", 15, 49, (const uint8_t*)Sinclair_S8x8, 0);

    // Динамічна кнопка SPATIAL
    lcd_draw_rectangle(96, 2, 90, 18);
    if (saved_spatial == 0) {
        lcd_print(spatial_names[0], 109, 7, (const uint8_t*)Sinclair_S8x8, 0);
    } else {
        lcd_print(spatial_names[1], 121, 7, (const uint8_t*)Sinclair_S8x8, 0);
    }

    // Динамічна кнопка SYS
    lcd_draw_rectangle(96, 23, 90, 18);
    lcd_print(sys_names[saved_sys], 113, 28, (const uint8_t*)Sinclair_S8x8, 0);

    lcd_draw_rectangle(96, 44, 90, 18);
    lcd_print("PHONO MM", 109, 49, (const uint8_t*)Sinclair_S8x8, 0);

    lcd_draw_rectangle(190, 2, 60, 60);
    animation_draw(ANIM_CAT3, 196, 8);

    draw_mute_badge(200, 4);

    lcd_update();
}

static void set_filters_menu(void) {
    lcd_clear();
    switch (filters_menu_pointer) {
        case 0: lcd_draw_rectangle(1, 1, 68, 20); break;
        case 1: lcd_draw_rectangle(1, 22, 68, 20); break;
        case 2: lcd_draw_rectangle(1, 43, 68, 20); break;
        case 3: lcd_draw_rectangle(69, 1, 68, 20); break;
        case 4: lcd_draw_rectangle(69, 22, 68, 20); break;
        case 5: lcd_draw_rectangle(69, 43, 68, 20); break;
    }
    lcd_draw_rectangle(2, 2, 66, 18);
    lcd_print("SILK", 19, 7, (const uint8_t*)Sinclair_S8x8, 0);
    lcd_draw_rectangle(2, 23, 66, 18);
    lcd_print("PURRITY", 7, 28, (const uint8_t*)Sinclair_S8x8, 0);
    lcd_draw_rectangle(2, 44, 66, 18);
    lcd_print("DRIVE", 15, 49, (const uint8_t*)Sinclair_S8x8, 0);
    lcd_draw_rectangle(70, 2, 66, 18);
    lcd_print("ATMOS", 83, 7, (const uint8_t*)Sinclair_S8x8, 0);
    lcd_draw_rectangle(70, 23, 66, 18);
    lcd_print("VILVET", 79, 28, (const uint8_t*)Sinclair_S8x8, 0);
    lcd_draw_rectangle(70, 44, 66, 18);
    lcd_print("DIRECT", 79, 49, (const uint8_t*)Sinclair_S8x8, 0);

    switch (saved_filter) {
        case 0: draw_active_square(62, 5); break;
        case 1: draw_active_square(62, 26); break;
        case 2: draw_active_square(62, 47); break;
        case 3: draw_active_square(130, 5); break;
        case 4: draw_active_square(130, 26); break;
        case 5: draw_active_square(130, 47); break;
    }

    lcd_draw_rectangle(139, 2, 111, 60);
    animation_draw(ANIM_CAT6, 170, 8);

    draw_mute_badge(200, 4);

    lcd_update();
}

static void set_eq_menu(void) {
    lcd_clear();
    lcd_print(eq_names[eq_menu_pointer], 118, 4, (const uint8_t*)Sinclair_S8x8, 0);

    for (int px = 117; px < 251; px += 4) {
        LCD_DrawPixel(&lcd, px, 29, 1);
    }

    int prev_x = 0, prev_y = 0;
    for (int i = 0; i < 10; i++) {
        int x = 121 + i * 14;
        int y = 62 - eq_presets[eq_menu_pointer][i];
        LCD_DrawRect(&lcd, x - 1, y - 1, 3, 3, 1);
        if (i > 0) LCD_DrawLine(&lcd, prev_x, prev_y, x, y, 1);
        prev_x = x; prev_y = y;
    }

    switch (eq_menu_pointer) {
        case 0: lcd_draw_rectangle(1, 1, 52, 20); break;
        case 1: lcd_draw_rectangle(1, 22, 52, 20); break;
        case 2: lcd_draw_rectangle(1, 43, 52, 20); break;
        case 3: lcd_draw_rectangle(55, 1, 58, 20); break;
        case 4: lcd_draw_rectangle(55, 22, 58, 20); break;
        case 5: lcd_draw_rectangle(55, 43, 58, 20); break;
    }
    lcd_draw_rectangle(2, 2, 50, 18);
    lcd_print("POP", 16, 7, (const uint8_t*)Sinclair_S8x8, 0);
    lcd_draw_rectangle(2, 23, 50, 18);
    lcd_print("ROCK", 12, 28, (const uint8_t*)Sinclair_S8x8, 0);
    lcd_draw_rectangle(2, 44, 50, 18);
    lcd_print("JAZZ", 11, 49, (const uint8_t*)Sinclair_S8x8, 0);
    lcd_draw_rectangle(56, 2, 56, 18);
    lcd_print("SYMPH", 65, 7, (const uint8_t*)Sinclair_S8x8, 0);
    lcd_draw_rectangle(56, 23, 56, 18);
    lcd_print("NATURE", 61, 28, (const uint8_t*)Sinclair_S8x8, 0);
    lcd_draw_rectangle(56, 44, 56, 18);
    lcd_print("B&T", 71, 49, (const uint8_t*)Sinclair_S8x8, 0);

    switch (saved_eq_preset) {
        case 0: draw_active_square(46, 5); break;
        case 1: draw_active_square(46, 26); break;
        case 2: draw_active_square(46, 47); break;
        case 3: draw_active_square(106, 5); break;
        case 4: draw_active_square(106, 26); break;
        case 5: draw_active_square(106, 47); break;
    }

    lcd_draw_rectangle(115, 2, 137, 60);

    draw_mute_badge(200, 4);

    lcd_update();
}

static void set_myspace_menu(void) {
    lcd_clear();
    
    // Автопрокрутка списку на 3 елементи
    int start_idx = myspace_menu_pointer;
    if (start_idx > 4) {
        start_idx = 4;
    } else if (start_idx > 0) {
        start_idx -= 1;
    }

    for (int i = 0; i < 3; i++) {
        int item_idx = start_idx + i;
        int y_offset = 1 + (i * 21);
        
        if (item_idx == myspace_menu_pointer) {
            lcd_draw_rectangle(1, y_offset, 150, 20);
        }
        lcd_draw_rectangle(2, y_offset + 1, 148, 18);
        
        lcd_print(myspace_names[item_idx], 6, y_offset + 6, (const uint8_t*)Sinclair_S8x8, 0);
        
        char val_str[16];
        if (item_idx == myspace_menu_pointer && is_myspace_editing) {
            snprintf(val_str, sizeof(val_str), ">%.2fm<", myspace_distances[item_idx]);
        } else {
            snprintf(val_str, sizeof(val_str), " %.2fm ", myspace_distances[item_idx]);
        }
        
        lcd_print(val_str, 95, y_offset + 6, (const uint8_t*)Sinclair_S8x8, 0);
    }

    lcd_draw_rectangle(155, 2, 97, 60);
    animation_draw(ANIM_CAT4, 160, 8);

    draw_mute_badge(200, 4);

    lcd_update();
}

static void set_phono_menu(void) {
    lcd_clear();
    switch (phono_menu_pointer) {
        case 0: lcd_draw_rectangle(1, 16, 152, 15); break;
        case 1: lcd_draw_rectangle(1, 46, 152, 15); break;
    }
    lcd_draw_rectangle(2, 2, 150, 13);
    lcd_print("000.015h30m", 42, 4, (const uint8_t*)Sinclair_S8x8, 0);
    lcd_draw_rectangle(2, 17, 150, 13);
    lcd_print("ERASE NEEDLE TIME", 10, 19, (const uint8_t*)Sinclair_S8x8, 0);
    lcd_draw_rectangle(2, 32, 150, 13);
    lcd_print("000.100h10m", 42, 34, (const uint8_t*)Sinclair_S8x8, 0);
    lcd_draw_rectangle(2, 47, 150, 13);
    lcd_print("ERASE TOTAL TIME", 14, 49, (const uint8_t*)Sinclair_S8x8, 0);

    lcd_draw_rectangle(155, 2, 95, 60);
    animation_draw(ANIM_CAT5, 178, 8);

    draw_mute_badge(200, 4);

    lcd_update();
}

void menu_update(void) {
    static int64_t last_anim_time = 0;
    int64_t now = esp_timer_get_time() / 1000;

    bool menu_changed = (menu_pointer != old_menu_pointer || 
                         main_menu_pointer != old_main_menu_pointer ||
                         filters_menu_pointer != old_filters_menu_pointer || 
                         eq_menu_pointer != old_eq_menu_pointer || 
                         myspace_menu_pointer != old_myspace_menu_pointer || 
                         phono_menu_pointer != old_phono_menu_pointer ||
                         is_muted != old_is_muted ||
                         is_myspace_editing != old_is_myspace_editing);

    if (menu_changed || (now - last_anim_time >= 100)) {
        last_anim_time = now;

        old_eq_menu_pointer = eq_menu_pointer;
        old_filters_menu_pointer = filters_menu_pointer;
        old_main_menu_pointer = main_menu_pointer;
        old_menu_pointer = menu_pointer;
        old_myspace_menu_pointer = myspace_menu_pointer;
        old_phono_menu_pointer = phono_menu_pointer;
        old_is_muted = is_muted;
        old_is_myspace_editing = is_myspace_editing;

        switch (menu_pointer) {
            case MAIN_MENU_NUM:    set_main_menu(); break;
            case FILTERS_MENU_NUM: set_filters_menu(); break;
            case EQ_MENU_NUM:      set_eq_menu(); break;
            case MYSPACE_MENU_NUM: set_myspace_menu(); break;
            case PHONO_MENU_NUM:   set_phono_menu(); break;    
        }
    }
}