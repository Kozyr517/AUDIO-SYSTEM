#include "screens.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lcd.h"
#include "animation.h"
#include "Font16x16.h"
#include "Sinclair_S8x8.h"
#include "autosearch.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define COLUM_SIZE 254
#define COLUM_FRAME_SKIP 25 

// Зовнішня черга спектроаналізатора (дані приходять з analizator.c)
extern QueueHandle_t g_fft_process_result_queue;

// Локальні масиви для плавності спектроаналізатора (падіння крапочок)
static uint8_t colum_data[COLUM_SIZE] = {0};
static uint8_t old_colum[COLUM_SIZE] = {0};
static uint8_t colum_peak_pos[COLUM_SIZE] = {0};
static uint8_t colum_timers[COLUM_SIZE] = {0};

// ==================== МАТЕМАТИЧНІ АНІМАЦІЇ СТАРТУ ТА ВИМКНЕННЯ ====================

// Магія математики при старті: сингулярність -> 3D інтерференція хвиль + вузол Ліссажу -> "CATZILLA"
void draw_boot_animation(void) {
    extern TypeDef_GP1247AI lcd;
    const int width = 252;
    const int cx = 126;
    const int cy = 32;

    // Фаза 1: Розгортання променя з центральної точки + розбіжні ударні дуги (~0.48 сек)
    const int ignite_frames = 12;
    for (int f = 0; f < ignite_frames; f++) {
        lcd_clear();
        float p = (float)f / (float)(ignite_frames - 1);
        int half_len = (int)((width / 2) * (p * p));
        LCD_DrawFastHLine(&lcd, (uint8_t)(cx - half_len), (uint8_t)cy, (uint8_t)(half_len * 2 + 1), 1);

        // Дві розбіжні хвильові дуги (еліпс у пропорціях VFD)
        float rx = 10.0f + p * 75.0f;
        float ry = 4.0f  + p * 26.0f;
        for (int a = 0; a < 36; a++) {
            float ang = (float)a * (6.28318f / 36.0f);
            int px = cx + (int)(rx * cosf(ang));
            int py = cy + (int)(ry * sinf(ang));
            if (px >= 0 && px <= width && py >= 1 && py <= 62) {
                LCD_DrawPixel(&lcd, (uint8_t)px, (uint8_t)py, 1);
            }
        }
        lcd_update();
        vTaskDelay(pdMS_TO_TICKS(40));
    }

    // Фаза 2: 3D-інтерференція 2 стоячих хвиль + обертання фігури Ліссажу (3:2) у центрі (~1.2 сек)
    const int magic_frames = 30;
    for (int f = 0; f < magic_frames; f++) {
        lcd_clear();
        float p = (float)f / (float)(magic_frames - 1);
        float ph = (float)f * 0.24f;
        // Плавне наростання і спад амплітуди (дзвін)
        float bell = sinf(p * 3.14159f);
        float wave_amp = 22.0f * (0.35f + 0.65f * bell);

        int prev_y1 = cy;
        for (int x = 0; x <= width; x++) {
            float u = (float)x / (float)width;
            float c = (u - 0.5f) * 2.0f;
            float env = expf(-c * c * 1.1f);

            // Стояча інтерференційна хвиля
            float y1_f = wave_amp * env * sinf(u * 18.85f - ph) * cosf(u * 6.28f + ph * 0.5f);
            int y1 = cy - (int)y1_f;
            if (y1 < 1) y1 = 1;
            if (y1 > 62) y1 = 62;

            if (x == 0) {
                LCD_DrawPixel(&lcd, (uint8_t)x, (uint8_t)y1, 1);
            } else {
                LCD_DrawLine(&lcd, (uint8_t)(x - 1), (uint8_t)prev_y1, (uint8_t)x, (uint8_t)y1, 1);
            }
            prev_y1 = y1;

            // Дзеркальна пунктирна гармоніка
            if (x % 3 == 0) {
                float y2_f = (wave_amp * 0.7f) * env * sinf(u * 12.57f + ph * 0.8f);
                int y2 = cy - (int)y2_f;
                if (y2 >= 1 && y2 <= 62) {
                    LCD_DrawPixel(&lcd, (uint8_t)x, (uint8_t)y2, 1);
                }
            }
        }

        // Обертовий 3D-вузол Ліссажу (співвідношення частот 3:2) у центрі екрана
        float liss_rx = 46.0f * bell + 12.0f;
        float liss_ry = 24.0f * bell + 4.0f;
        int prev_lx = cx, prev_ly = cy;
        const int steps = 48;
        for (int s = 0; s <= steps; s++) {
            float a = (float)s * (6.28318f / (float)steps);
            int lx = cx + (int)(liss_rx * sinf(3.0f * a + ph));
            int ly = cy + (int)(liss_ry * sinf(2.0f * a));
            if (lx < 0) lx = 0;
            if (lx > width) lx = width;
            if (ly < 1) ly = 1;
            if (ly > 62) ly = 62;
            if (s > 0 && (s % 2 == 0)) {
                LCD_DrawLine(&lcd, (uint8_t)prev_lx, (uint8_t)prev_ly, (uint8_t)lx, (uint8_t)ly, 1);
            }
            prev_lx = lx;
            prev_ly = ly;
        }

        lcd_update();
        vTaskDelay(pdMS_TO_TICKS(40));
    }

    // Фаза 3: Розходження хвиль у верхню й нижню рамку та поява логотипу CATZILLA (~0.9 сек)
    const int title_frames = 22;
    for (int f = 0; f < title_frames; f++) {
        lcd_clear();
        float p = (float)f / (float)(title_frames - 1);
        float ph = (float)f * 0.30f;
        int offset = 6 + (int)(18.0f * (p < 0.4f ? (p / 0.4f) : 1.0f));
        float ripple = 4.0f * (1.0f - p);

        for (int x = 10; x <= width - 10; x += 2) {
            float u = (float)x / (float)width;
            int dy = (int)(ripple * sinf(u * 25.0f - ph));
            int yt = cy - offset + dy;
            int yb = cy + offset - dy;
            if (yt >= 1 && yt <= 62) LCD_DrawPixel(&lcd, (uint8_t)x, (uint8_t)yt, 1);
            if (yb >= 1 && yb <= 62) LCD_DrawPixel(&lcd, (uint8_t)x, (uint8_t)yb, 1);
        }

        if (f >= 4) {
            // "CATZILLA" = 8 символів * 16 px = 128 px -> центр x = (252 - 128) / 2 = 62
            lcd_print("CATZILLA", 62, 24, (const uint8_t*)Font16x16, 0);
        }

        lcd_update();
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

// Магія математики при вимкненні: гравітаційна спіраль Фібоначчі + затухання хвиль у центральну сингулярність
void draw_shutdown_animation(void) {
    extern TypeDef_GP1247AI lcd;
    const int width = 252;
    const int cx = 126;
    const int cy = 32;

    // Фаза 1: Стягування спіралі Фібоначчі + затухання 2 гармонік у горизонтальну струну (~1.5 сек)
    const int wave_frames = 38;
    for (int f = 0; f < wave_frames; f++) {
        lcd_clear();
        float prog = (float)f / (float)(wave_frames - 1);
        float decay = (1.0f - prog) * (1.0f - prog);
        float amp = 23.0f * decay;
        float phase = (float)f * 0.32f;

        // Затухаюча звукова хвиля
        int prev_y1 = cy;
        for (int x = 0; x <= width; x++) {
            float u = (float)x / (float)width;
            float c = (u - 0.5f) * 2.0f;
            float env = expf(-c * c * 1.4f);

            float y1_f = amp * env * sinf(u * 18.85f - phase);
            int y1 = cy - (int)y1_f;
            if (y1 < 1) y1 = 1;
            if (y1 > 62) y1 = 62;

            if (x == 0) {
                LCD_DrawPixel(&lcd, (uint8_t)x, (uint8_t)y1, 1);
            } else {
                LCD_DrawLine(&lcd, (uint8_t)(x - 1), (uint8_t)prev_y1, (uint8_t)x, (uint8_t)y1, 1);
            }
            prev_y1 = y1;
        }

        // Обертова спіраль Фібоначчі (золотий кут 2.39996 рад), що втягується в центр
        if (decay > 0.04f) {
            const int dots = 42;
            for (int k = 1; k <= dots; k++) {
                float r_norm = sqrtf((float)k / (float)dots) * decay;
                float ang = (float)k * 2.399963f + phase * 1.3f;
                int px = cx + (int)(r_norm * 95.0f * cosf(ang));
                int py = cy + (int)(r_norm * 29.0f * sinf(ang));
                if (px >= 0 && px <= width && py >= 1 && py <= 62) {
                    LCD_DrawPixel(&lcd, (uint8_t)px, (uint8_t)py, 1);
                }
            }
        }

        lcd_update();
        vTaskDelay(pdMS_TO_TICKS(40));
    }

    // Фаза 2: Стискання струни з обох країв у центральну зірку та згасання (~0.6 сек)
    const int collapse_frames = 15;
    for (int f = 0; f < collapse_frames; f++) {
        lcd_clear();
        float prog = (float)f / (float)(collapse_frames - 1);
        int half_len = (int)((width / 2) * (1.0f - prog * prog));
        int x_start = cx - half_len;
        int len = half_len * 2 + 1;

        LCD_DrawFastHLine(&lcd, (uint8_t)x_start, (uint8_t)cy, (uint8_t)len, 1);
        if (f == collapse_frames - 2) {
            // Короткий іскровий хрестик у центрі перед згасанням
            LCD_DrawFastVLine(&lcd, (uint8_t)cx, (uint8_t)(cy - 4), 9, 1);
            LCD_FillRect(&lcd, (uint8_t)(cx - 1), (uint8_t)(cy - 1), 3, 3, 1);
        } else if (f == collapse_frames - 1) {
            LCD_DrawPixel(&lcd, (uint8_t)cx, (uint8_t)cy, 1);
        }
        lcd_update();
        vTaskDelay(pdMS_TO_TICKS(40));
    }

    lcd_clear();
    lcd_update();
    vTaskDelay(pdMS_TO_TICKS(150));
}

// Вивід назви активного входу на 2 сек у правому верхньому кутку (там само, де MUTE)
static void draw_active_channel_overlay(void) {
    const char *ch_text = autosearch_get_active_popup_text();
    if (ch_text == NULL) return;

    int text_w = (int)strlen(ch_text) * 8;
    int x = 250 - text_w;
    if (x < 0) x = 0;

    // Очищаємо прямокутник під текстом, щоб смужки спектроаналізатора не накладалися
    extern TypeDef_GP1247AI lcd;
    LCD_FillRect(&lcd, (uint8_t)(x > 2 ? x - 2 : 0), 0, (uint8_t)(text_w + 4), 11, 0);
    lcd_print(ch_text, (uint8_t)x, 2, (const uint8_t*)Sinclair_S8x8, 0);
}

void draw_idle_cat2_frame(void) {
    lcd_clear();
    animation_draw(ANIM_CAT2, 63, 8);
    draw_active_channel_overlay();
    lcd_update();
}

void draw_mute_frame(void) {
    lcd_clear();
    animation_draw(ANIM_CAT2, 63, 8);
    // Відступ від правого краю
    lcd_print("MUTE", 218, 2, (const uint8_t*)Sinclair_S8x8, 0);
    lcd_update();
}

// ==================== СПЕКТРОАНАЛІЗАТОР (ДИНАМІЧНИЙ) ====================

void draw_spectrum_analyzer_frame(void) {
    if (g_fft_process_result_queue == NULL) return;

    if (xQueueReceive(g_fft_process_result_queue, colum_data, 0) == pdTRUE) {
        lcd_clear();

        for (size_t i = 0; i < COLUM_SIZE; i++) {
            if (colum_data[i] > old_colum[i]) {
                old_colum[i] = colum_data[i];
                if (colum_data[i] >= colum_peak_pos[i]) {
                    colum_peak_pos[i] = colum_data[i];
                    colum_timers[i] = COLUM_FRAME_SKIP;
                }
            } else {
                if (old_colum[i] > 0) old_colum[i]--;

                if (colum_timers[i] > 0) colum_timers[i]--;
                if (colum_timers[i] == 0 && colum_peak_pos[i] > 0) {
                    colum_peak_pos[i]--;
                }
            }
        }

        // Малювання смужок та пікових крапочок
        for (size_t i = 0; i < COLUM_SIZE - 1; i++) {
            lcd_set_dot(i, colum_peak_pos[i]);
            lcd_draw_colum(i, old_colum[i]);
        }
        draw_active_channel_overlay();
        lcd_update();
    }
}

// ==================== ПОПАП ГУЧНОСТІ ====================

void draw_volume_popup(uint8_t master_volume) {
    lcd_clear();
    lcd_draw_rectangle(28, 10, 198, 44);
    lcd_draw_rectangle(30, 12, 194, 40);

    char vol_str[16];
    snprintf(vol_str, sizeof(vol_str), "VOLUME: %d%%", master_volume);
    lcd_print(vol_str, 85, 18, (const uint8_t*)Sinclair_S8x8, 0);

    uint16_t bar_width = (master_volume * 178) / 100;
    if (bar_width > 0) {
        for (uint8_t h = 32; h <= 42; h++) {
            // Використовується зовнішня функція малювання ліній з lcd.h
            extern TypeDef_GP1247AI lcd;
            LCD_DrawFastHLine(&lcd, 38, h, bar_width, 1);
        }
    }
    lcd_update();
}