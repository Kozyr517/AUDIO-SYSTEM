#include "menu.h"
#include "lcd.h"
#include "Font16x16.h"
#include "Sinclair_S8x8.h"
#include "animation.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "MENU_SETTINGS";

uint8_t menu_pointer = 0;
uint8_t main_menu_pointer = 0;
uint8_t filters_menu_pointer = 0;
uint8_t eq_menu_pointer = 0;
uint8_t balance_menu_pointer = 0;
uint8_t phono_menu_pointer = 0;

uint8_t old_menu_pointer = 255;
uint8_t old_main_menu_pointer = 0;
uint8_t old_filters_menu_pointer = 0;
uint8_t old_eq_menu_pointer = 0;
uint8_t old_balance_menu_pointer = 0;
uint8_t old_phono_menu_pointer = 0;

// ==================== ЗБЕРЕЖЕНІ НАЛАШТУВАННЯ В RAM ====================
uint8_t saved_eq_preset = 0;
uint8_t saved_filter    = 0;
uint8_t saved_balance   = 0;
uint8_t saved_spatial_3d = 0;
uint8_t saved_night_mode = 0;

const char* eq_names[6]      = {"POP", "ROCK", "JAZZ", "SYMPH", "NATURE", "BASS"};
const char* filter_names[6]  = {"SILK", "PURRITY", "DRIVE", "ATMOS", "VILVET", "DIRECT"};
const char* balance_names[3] = {"VOL", "L/R", "F/B"};

// Малювання заповненого квадрата/кубика 4x4 пікселі у верхньому правому кутку комірки
static void draw_active_square(int x, int y) {
    for (int dy = 0; dy < 4; dy++) {
        for (int dx = 0; dx < 4; dx++) {
            LCD_DrawPixel(&lcd, x + dx, y + dy, 1);
        }
    }
}

// ==================== ФУНКЦІЇ ЗБЕРЕЖЕННЯ ТА ЗЧИТУВАННЯ EEPROM ====================
void eeprom_save_all_settings(void) {
    ESP_LOGI(TAG, "===> EEPROM SAVE: EQ=%d (%s), Filter=%d (%s), Balance=%d (%s), Spatial3D=%s, NightMode=%s", 
             saved_eq_preset, eq_names[saved_eq_preset], 
             saved_filter, filter_names[saved_filter], 
             saved_balance, balance_names[saved_balance],
             saved_spatial_3d ? "ON" : "OFF",
             saved_night_mode ? "ON" : "OFF");

    /* --- КОД ДЛЯ ЗОВНІШНЬОЇ EEPROM (I2C/SPI) ---
    uint8_t data_to_write[5] = { saved_eq_preset, saved_filter, saved_balance, saved_spatial_3d, saved_night_mode };
    // i2c_master_write_to_device(...);
    ---------------------------------------------- */
}

void eeprom_load_all_settings(void) {
    /* --- КОД ДЛЯ ЗЧИТУВАННЯ З ЗОВНІШНЬОЇ EEPROM --- */
    ESP_LOGI(TAG, "EEPROM settings loaded into RAM");
}

static const uint8_t eq_pop[10]    = {29, 40, 44, 45, 41, 30, 28, 28, 29, 29};
static const uint8_t eq_rock[10]   = {45, 40, 23, 19, 26, 39, 47, 50, 50, 50};
static const uint8_t eq_jazz[10]   = {40, 38, 32, 33, 31, 35, 39, 41, 43, 44};
static const uint8_t eq_symph[10]  = {49, 49, 42, 42, 33, 24, 24, 24, 33, 33};
static const uint8_t eq_nature[10] = {33, 33, 33, 33, 33, 33, 33, 33, 33, 33};
static const uint8_t eq_bass[10]   = {48, 48, 48, 42, 35, 25, 18, 15, 14, 14};

static const uint8_t* eq_presets[6] = {eq_pop, eq_rock, eq_jazz, eq_symph, eq_nature, eq_bass};

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
    lcd_print("VOLUME/BAL", 7, 49, (const uint8_t*)Sinclair_S8x8, 0);
    lcd_draw_rectangle(96, 2, 90, 18);
    lcd_print("SPATIAL 3D", 101, 7, (const uint8_t*)Sinclair_S8x8, 0);
    lcd_draw_rectangle(96, 23, 90, 18);
    lcd_print("NIGHT MOD", 105, 28, (const uint8_t*)Sinclair_S8x8, 0);
    lcd_draw_rectangle(96, 44, 90, 18);
    lcd_print("PHONO MM", 109, 49, (const uint8_t*)Sinclair_S8x8, 0);

    // Малювання квадратиків для збережених прямих режимів ON/OFF у Головному меню
    if (saved_spatial_3d) {
        draw_active_square(180, 5);
    }
    if (saved_night_mode) {
        draw_active_square(180, 26);
    }

    lcd_draw_rectangle(190, 2, 60, 60);
    animation_draw(ANIM_CAT3, 196, 8);
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
    lcd_print("BASS", 69, 49, (const uint8_t*)Sinclair_S8x8, 0);

    switch (saved_eq_preset) {
        case 0: draw_active_square(46, 5); break;
        case 1: draw_active_square(46, 26); break;
        case 2: draw_active_square(46, 47); break;
        case 3: draw_active_square(106, 5); break;
        case 4: draw_active_square(106, 26); break;
        case 5: draw_active_square(106, 47); break;
    }

    lcd_draw_rectangle(115, 2, 137, 60);
    lcd_update();
}

static void set_balance_menu(void) {
    lcd_clear();
    lcd_print(balance_names[balance_menu_pointer], 118, 4, (const uint8_t*)Sinclair_S8x8, 0);
    switch (balance_menu_pointer) {
        case 0: lcd_draw_rectangle(1, 1, 112, 20); break;
        case 1: lcd_draw_rectangle(1, 22, 112, 20); break;
        case 2: lcd_draw_rectangle(1, 43, 112, 20); break;
    }
    lcd_draw_rectangle(2, 2, 110, 18);
    lcd_print("MASTER VOLUME", 11, 7, (const uint8_t*)Sinclair_S8x8, 0);
    lcd_draw_rectangle(2, 23, 110, 18);
    lcd_print("BALANCE LEFT/RIGHT", 7, 28, (const uint8_t*)Sinclair_S8x8, 0);
    lcd_draw_rectangle(2, 44, 110, 18);
    lcd_print("BALANCE FRONT/BACK", 7, 49, (const uint8_t*)Sinclair_S8x8, 0);

    switch (saved_balance) {
        case 0: draw_active_square(106, 5); break;
        case 1: draw_active_square(106, 26); break;
        case 2: draw_active_square(106, 47); break;
    }

    lcd_draw_rectangle(115, 2, 137, 60);
    animation_draw(ANIM_CAT4, 160, 8);
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
    lcd_update();
}

void menu_update(void) {
    static int64_t last_anim_time = 0;
    int64_t now = esp_timer_get_time() / 1000;

    bool menu_changed = (menu_pointer != old_menu_pointer || 
                         main_menu_pointer != old_main_menu_pointer ||
                         filters_menu_pointer != old_filters_menu_pointer || 
                         eq_menu_pointer != old_eq_menu_pointer || 
                         balance_menu_pointer != old_balance_menu_pointer || 
                         phono_menu_pointer != old_phono_menu_pointer);

    if (menu_changed || (now - last_anim_time >= 100)) {
        last_anim_time = now;

        old_eq_menu_pointer = eq_menu_pointer;
        old_filters_menu_pointer = filters_menu_pointer;
        old_main_menu_pointer = main_menu_pointer;
        old_menu_pointer = menu_pointer;
        old_balance_menu_pointer = balance_menu_pointer;
        old_phono_menu_pointer = phono_menu_pointer;

        switch (menu_pointer) {
            case MAIN_MENU_NUM:    set_main_menu(); break;
            case FILTERS_MENU_NUM: set_filters_menu(); break;
            case EQ_MENU_NUM:      set_eq_menu(); break;
            case BALANCE_MENU_NUM: set_balance_menu(); break;
            case PHONO_MENU_NUM:   set_phono_menu(); break;    
        }
    }
}