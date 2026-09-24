#include <stdio.h>
#include <stdlib.h> // Для функції abs()
#include "menu.h"
#include "lcd.h"
#include "gp1247ai.h"
#include "Font16x16.h"
#include "Sinclair_S8x8.h"
#include "animation.h"
#include "esp_timer.h"
#include "esp_log.h"

// Глобальний об'єкт дисплея з lcd.c
extern TypeDef_GP1247AI lcd;

// Зовнішні змінні з buttons.c / main.c
extern volatile bool is_muted;

// Зовнішній масив значень MySpace (12 елементів: 0-5 Distance, 6-11 Gain)
extern float myspace_values[12];

uint8_t menu_pointer = 0;
uint8_t main_menu_pointer = 0;
uint8_t filters_menu_pointer = 0;
uint8_t eq_menu_pointer = 0;
uint8_t phono_menu_pointer = 0;

uint8_t old_menu_pointer = 255;
uint8_t old_main_menu_pointer = 0;
uint8_t old_filters_menu_pointer = 0;
uint8_t old_eq_menu_pointer = 0;
uint8_t old_phono_menu_pointer = 0;

static bool old_is_muted = false;

// ==================== НОВІ ЗМІННІ ДЛЯ MY SPACE ====================
uint8_t myspace_speaker_pointer = 0; // 0..5 (FL, C, SUB, FR, RL, RR)
uint8_t myspace_param_pointer = 0;   // 0 = DISTANCE, 1 = GAIN
uint8_t myspace_depth = 1;           // 1 = Вибір динаміка, 2 = Вибір параметра, 3 = Редагування цифри

static uint8_t old_myspace_speaker_pointer = 255;
static uint8_t old_myspace_param_pointer = 255;
static uint8_t old_myspace_depth = 255;

// Назви динаміків (відповідають індексам 0-5)
const char *myspace_speaker_names[6] = {
    "FRONT L", "CENTER", "SUBWOOFER", "FRONT R", "REAR L", "REAR R"
};
// ==================================================================

// ==================== ЗБЕРЕЖЕНІ НАЛАШТУВАННЯ В RAM ====================
uint8_t saved_eq_preset  = 0;
uint8_t saved_filter     = 0;
uint8_t saved_spatial    = 0;
uint8_t saved_sys        = 0;

const char* eq_names[6]      = {"POP", "ROCK", "JAZZ", "SYMPH", "NATURE", "B&T"};
const char* filter_names[6]  = {"SILK", "PURRITY", "DRIVE", "ATMOS", "VILVET", "DIRECT"};
const char* spatial_names[2] = {"SURROUND", "MUSIC"};
const char* sys_names[2]     = {"SYS 2.1", "SYS 5.1"};


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
        lcd_draw_rectangle(x, y, 42, 12);
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

    lcd_draw_rectangle(96, 2, 90, 18);
    if (saved_spatial == 0) {
        lcd_print(spatial_names[0], 109, 7, (const uint8_t*)Sinclair_S8x8, 0);
    } else {
        lcd_print(spatial_names[1], 121, 7, (const uint8_t*)Sinclair_S8x8, 0);
    }

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

/* 
Закоментовано, щоб компілятор не видавав попередження. 
Якщо знадобляться в майбутньому - просто розкоментуй.
static void draw_triangle_up(int x, int y) {
    LCD_DrawPixel(&lcd, x, y, 1);
    LCD_DrawPixel(&lcd, x - 1, y + 1, 1);
    LCD_DrawPixel(&lcd, x, y + 1, 1);
    LCD_DrawPixel(&lcd, x + 1, y + 1, 1);
    LCD_DrawPixel(&lcd, x - 2, y + 2, 1);
    LCD_DrawPixel(&lcd, x - 1, y + 2, 1);
    LCD_DrawPixel(&lcd, x, y + 2, 1);
    LCD_DrawPixel(&lcd, x + 1, y + 2, 1);
    LCD_DrawPixel(&lcd, x + 2, y + 2, 1);
}

static void draw_triangle_down(int x, int y) {
    LCD_DrawPixel(&lcd, x - 2, y, 1);
    LCD_DrawPixel(&lcd, x - 1, y, 1);
    LCD_DrawPixel(&lcd, x, y, 1);
    LCD_DrawPixel(&lcd, x + 1, y, 1);
    LCD_DrawPixel(&lcd, x + 2, y, 1);
    LCD_DrawPixel(&lcd, x - 1, y + 1, 1);
    LCD_DrawPixel(&lcd, x, y + 1, 1);
    LCD_DrawPixel(&lcd, x + 1, y + 1, 1);
    LCD_DrawPixel(&lcd, x, y + 2, 1);
}
*/

static void draw_mini_cat(int x, int y) {
    static const uint16_t cat_bmp[9] = {
        0b10000000001,
        0b11000000011,
        0b11100000111,
        0b10111111101,
        0b10000000001,
        0b10100000101,
        0b10000100001,
        0b01001110010,
        0b00111111100 
    };
    
    for (int row = 0; row < 9; row++) {
        for (int col = 0; col < 11; col++) {
            if (cat_bmp[row] & (1 << (10 - col))) {
                lcd_draw_rectangle(x + col, y + row, 1, 1);
            }
        }
    }
}

static void draw_dashed_line(int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    int count = 0;

    while (1) {
        if (count % 3 != 0) { 
            lcd_draw_rectangle(x0, y0, 1, 1);
        }
        count++;
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void draw_sound_wave(int speaker_idx) {
    switch (speaker_idx) {
        case 0: // FRONT L
            lcd_draw_rectangle(167, 16, 1, 3);
            lcd_draw_rectangle(168, 15, 2, 1);
            lcd_draw_rectangle(171, 19, 1, 4);
            lcd_draw_rectangle(172, 18, 3, 1);
            break;
        case 1: // CENTER
            lcd_draw_rectangle(181, 21, 9, 1);
            lcd_draw_rectangle(179, 25, 13, 1);
            break;
        case 2: // SUBWOOFER
            lcd_draw_rectangle(204, 21, 9, 1);
            lcd_draw_rectangle(202, 25, 13, 1);
            break;
        case 3: // FRONT R
            lcd_draw_rectangle(227, 16, 1, 3);
            lcd_draw_rectangle(225, 15, 2, 1);
            lcd_draw_rectangle(223, 19, 1, 4);
            lcd_draw_rectangle(220, 18, 3, 1);
            break;
        case 4: // REAR L
            lcd_draw_rectangle(167, 45, 1, 3);
            lcd_draw_rectangle(168, 48, 2, 1);
            lcd_draw_rectangle(171, 41, 1, 4);
            lcd_draw_rectangle(172, 45, 3, 1);
            break;
        case 5: // REAR R
            lcd_draw_rectangle(227, 45, 1, 3);
            lcd_draw_rectangle(225, 48, 2, 1);
            lcd_draw_rectangle(223, 41, 1, 4);
            lcd_draw_rectangle(220, 45, 3, 1);
            break;
    }
}

static void set_myspace_menu(void) {
    lcd_clear();
    char val_str[24]; // Буфер для тексту

    // Отримуємо поточні значення для обраного динаміка
    float dist_val = myspace_values[myspace_speaker_pointer];       // Індекси 0..5
    float gain_val = myspace_values[myspace_speaker_pointer + 6];   // Індекси 6..11

    // Прапорець для блимання (змінює стан кожні 400 мс)
    bool blink = (xTaskGetTickCount() / pdMS_TO_TICKS(400)) % 2 == 0;

    // ==========================================
    // ЛІВИЙ БЛОК: Загальна рамка та параметри
    // ==========================================
    
    // Одна велика рамка, за висотою як схема кімнати (Y=2, H=60)
    lcd_draw_rectangle(1, 2, 138, 60); 

    // 1. Рядок ВИБОРУ ДИНАМІКА (Y = 8)
    lcd_print("SPK:", 6, 8, (const uint8_t*)Sinclair_S8x8, 0);
    
    // Блимання назви динаміка, якщо ми на рівні 0
    if (myspace_depth != 0 || blink) {
        lcd_print(myspace_speaker_names[myspace_speaker_pointer], 45, 8, (const uint8_t*)Sinclair_S8x8, 0);
    }
    
    // Пунктирна лінія-розділювач під динаміком (статична)
    for(int x = 2; x < 138; x += 4) {
        LCD_DrawPixel(&lcd, x, 20, 1);
    }

    // 2. Рядок ГУЧНОСТІ (Y = 26)
    lcd_print("GAIN:", 6, 26, (const uint8_t*)Sinclair_S8x8, 0);
    
    char left_cur_gain = ' ';
    char right_cur_gain = ' ';
    
    if (myspace_depth >= 1 && myspace_param_pointer == 1) {
        // Якщо параметр обрано (Рівень 1) або редагується (Рівень 2 + блимання)
        if (myspace_depth == 1 || (myspace_depth == 2 && blink)) {
            left_cur_gain = '>';
            right_cur_gain = '<';
        }
    }
    snprintf(val_str, sizeof(val_str), "%c%+2.1f dB%c", left_cur_gain, gain_val, right_cur_gain);
    lcd_print(val_str, 50, 26, (const uint8_t*)Sinclair_S8x8, 0);

    // 3. Рядок ДИСТАНЦІЇ (Y = 44)
    lcd_print("DIST:", 6, 44, (const uint8_t*)Sinclair_S8x8, 0);
    
    char left_cur_dist = ' ';
    char right_cur_dist = ' ';
    
    if (myspace_depth >= 1 && myspace_param_pointer == 0) {
        // Якщо параметр обрано (Рівень 1) або редагується (Рівень 2 + блимання)
        if (myspace_depth == 1 || (myspace_depth == 2 && blink)) {
            left_cur_dist = '>';
            right_cur_dist = '<';
        }
    }
    snprintf(val_str, sizeof(val_str), "%c%.1f m %c", left_cur_dist, dist_val, right_cur_dist);
    lcd_print(val_str, 50, 44, (const uint8_t*)Sinclair_S8x8, 0);


    // ==========================================
    // ПРАВИЙ БЛОК: Схема приміщення 5.1
    // ==========================================
    lcd_draw_rectangle(142, 2, 111, 60); // Основна рамка кімнати

    lcd_draw_rectangle(146, 5, 19, 13);  lcd_print("LF", 149, 7, (const uint8_t*)Sinclair_S8x8, 0);
    lcd_draw_rectangle(176, 5, 19, 13);  lcd_print("C",  182, 7, (const uint8_t*)Sinclair_S8x8, 0);
    lcd_draw_rectangle(199, 5, 19, 13);  lcd_print("SW", 201, 7, (const uint8_t*)Sinclair_S8x8, 0);
    lcd_draw_rectangle(230, 5, 19, 13);  lcd_print("RF", 232, 7, (const uint8_t*)Sinclair_S8x8, 0);

    draw_mini_cat(192, 42);

    lcd_draw_rectangle(146, 46, 19, 13); lcd_print("RL", 148, 48, (const uint8_t*)Sinclair_S8x8, 0);
    lcd_draw_rectangle(230, 46, 19, 13); lcd_print("RR", 232, 48, (const uint8_t*)Sinclair_S8x8, 0);

    // ==========================================
    // АНІМАЦІЇ ВИДІЛЕННЯ КВАДРАТІВ ДИНАМІКІВ
    // ==========================================
    
    // Визначаємо координати для зовнішньої рамки виділеного динаміка
    int fx = 0, fy = 0;
    switch (myspace_speaker_pointer) {
        case 0: fx = 144; fy = 3; break;  // LF
        case 1: fx = 174; fy = 3; break;  // C
        case 2: fx = 197; fy = 3; break;  // SW
        case 3: fx = 228; fy = 3; break;  // RF
        case 4: fx = 144; fy = 44; break; // RL
        case 5: fx = 228; fy = 44; break; // RR
    }

    if (myspace_depth == 0) {
        // Рівень 0: обираємо динамік - рамка БЛИМАЄ
        if (blink) {
            lcd_draw_rectangle(fx, fy, 23, 17); // Рамка на 2px ширша/вища за динамік
        }
    } 
    else if (myspace_depth >= 1) {
        // Рівень 1 і 2: динамік обрано, налаштовуємо параметри - рамка ГОРИТЬ ПОСТІЙНО
        lcd_draw_rectangle(fx, fy, 23, 17);

        if (myspace_param_pointer == 1) {
            // Редагуємо гучність - малюємо скобочки (хвилі)
            draw_sound_wave(myspace_speaker_pointer);
        } else if (myspace_param_pointer == 0) {
            // Редагуємо дистанцію - малюємо лінію до центру
            switch (myspace_speaker_pointer) {
                case 0: draw_dashed_line(155, 18, 192, 42); break;
                case 1: draw_dashed_line(185, 18, 196, 40); break;
                case 2: draw_dashed_line(208, 18, 198, 40); break;
                case 3: draw_dashed_line(239, 18, 202, 42); break;
                case 4: draw_dashed_line(166, 52, 190, 48); break;
                case 5: draw_dashed_line(228, 52, 204, 48); break;
            }
        }
    }

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
                         phono_menu_pointer != old_phono_menu_pointer ||
                         is_muted != old_is_muted ||
                         myspace_speaker_pointer != old_myspace_speaker_pointer ||
                         myspace_param_pointer != old_myspace_param_pointer ||
                         myspace_depth != old_myspace_depth); 

    if (menu_changed || (now - last_anim_time >= 100)) {
        last_anim_time = now;

        old_eq_menu_pointer = eq_menu_pointer;
        old_filters_menu_pointer = filters_menu_pointer;
        old_main_menu_pointer = main_menu_pointer;
        old_menu_pointer = menu_pointer;
        old_phono_menu_pointer = phono_menu_pointer;
        old_is_muted = is_muted;
        old_myspace_speaker_pointer = myspace_speaker_pointer;
        old_myspace_param_pointer = myspace_param_pointer;
        old_myspace_depth = myspace_depth;

        switch (menu_pointer) {
            case MAIN_MENU_NUM:    set_main_menu(); break;
            case FILTERS_MENU_NUM: set_filters_menu(); break;
            case EQ_MENU_NUM:      set_eq_menu(); break;
            case MYSPACE_MENU_NUM: set_myspace_menu(); break;
            case PHONO_MENU_NUM:   set_phono_menu(); break;    
        }
    }
}