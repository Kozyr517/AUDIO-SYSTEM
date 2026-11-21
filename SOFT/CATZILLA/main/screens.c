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

#define COLUM_SIZE 254
#define COLUM_FRAME_SKIP 25 

// Зовнішня черга спектроаналізатора (дані приходять з analizator.c)
extern QueueHandle_t g_fft_process_result_queue;

// Локальні масиви для плавності спектроаналізатора (падіння крапочок)
static uint8_t colum_data[COLUM_SIZE] = {0};
static uint8_t old_colum[COLUM_SIZE] = {0};
static uint8_t colum_peak_pos[COLUM_SIZE] = {0};
static uint8_t colum_timers[COLUM_SIZE] = {0};

// ==================== БАЗОВІ АНІМАЦІЇ ====================

void draw_boot_animation(void) {
    for (int16_t x = -130; x <= 253; x += 6) {
        lcd_clear();
        animation_draw(ANIM_CAT1, x, 8);
        lcd_update();
        vTaskDelay(pdMS_TO_TICKS(40)); 
    }
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