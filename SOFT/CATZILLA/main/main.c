#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "app_state.h"
#include "cooling.h"
#include "buttons.h"
#include "menu.h"
#include "lcd.h"
#include "analizator.h"
#include "animation.h"
#include "Sinclair_S8x8.h"


static const char *TAG = "MAIN";

// ==================== ПІНИ ТА КОНФІГУРАЦІЯ ====================
#define PIN_VSYS_EN         GPIO_NUM_4
#define PIN_BLE_EN          GPIO_NUM_5
#define PIN_EN_POW_ADAU     GPIO_NUM_10
#define PIN_EN_ALL_POWER    GPIO_NUM_21
#define PIN_EN_ADDR_LED     GPIO_NUM_47

#define VOL_TIMEOUT_MS      3000
#define COLUM_SIZE          254
#define COLUM_FRAME_SKIP    25 

// ==================== ІНІЦІАЛІЗАЦІЯ СТАНУ ====================
volatile app_state_t current_state = STATE_BOOT;
uint8_t master_volume = 10;
TickType_t last_vol_activity_tick = 0;

// ==================== ГЛОБАЛЬНІ ЗМІННІ ====================
extern TypeDef_GP1247AI lcd;
extern uint8_t is_input_sig_flag;

QueueHandle_t g_fft_process_result_queue = NULL;

static uint8_t colum_data[COLUM_SIZE] = {0};
static uint8_t old_colum[COLUM_SIZE] = {0};
static uint8_t colum_peak_pos[COLUM_SIZE] = {0};
static uint8_t colum_timers[COLUM_SIZE] = {0};

// ==================== ГРАФІКА ТА ІНТЕРФЕЙС ====================
void draw_boot_animation(void) {
   for (int16_t x = -130; x <= 253; x += 6) {
        lcd_clear();
        animation_draw(ANIM_CAT1, x, 8);
        lcd_update(); 
        vTaskDelay(pdMS_TO_TICKS(40));
    }

    lcd_clear();
    lcd_print("WELCOME", (253 - (6 * 16)) / 2, 35, (const uint8_t*)Sinclair_S8x8, 0);
    lcd_update();
    vTaskDelay(pdMS_TO_TICKS(1500));
}

void draw_idle_cat2_frame(void) {
    lcd_clear();
    animation_draw(ANIM_CAT2, 63, 8);
    lcd_update();
}

void draw_spectrum_analyzer_frame(void) {
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

        for (size_t i = 0; i < COLUM_SIZE - 1; i++) {
            lcd_set_dot(i, colum_peak_pos[i]);
            lcd_draw_colum(i, old_colum[i]);
        }

        lcd_update();
    }
}

void draw_volume_popup(void) {
    lcd_clear();
    lcd_draw_rectangle(28, 10, 198, 44);
    lcd_draw_rectangle(30, 12, 194, 40);

    char vol_str[16];
    snprintf(vol_str, sizeof(vol_str), "VOLUME: %d%%", master_volume);
    lcd_print(vol_str, 85, 18, (const uint8_t*)Sinclair_S8x8, 0);

    uint16_t bar_width = (master_volume * 178) / 100;
    if (bar_width > 0) {
        for (uint8_t h = 32; h <= 42; h++) {
            LCD_DrawFastHLine(&lcd, 38, h, bar_width, 1);
        }
    }
    
    lcd_update();
}

// ==================== ЗАДАЧА ДИСПЛЕЯ ====================
static void ui_display_task(void *pvParameters) {
    current_state = STATE_BOOT;
    draw_boot_animation();

    current_state = (is_input_sig_flag == 1) ? STATE_SPECTRUM : STATE_IDLE_CAT2;

    while (1) {
        switch (current_state) {
            case STATE_IDLE_CAT2:
                if (is_input_sig_flag == 1) {
                    current_state = STATE_SPECTRUM;
                } else {
                    draw_idle_cat2_frame();
                    vTaskDelay(pdMS_TO_TICKS(60));
                }
                break;

            case STATE_SPECTRUM:
                if (is_input_sig_flag == 0) {
                    current_state = STATE_IDLE_CAT2;
                } else {
                    draw_spectrum_analyzer_frame();
                    vTaskDelay(pdMS_TO_TICKS(20));
                }
                break;

            case STATE_VOLUME_POPUP:
                draw_volume_popup();
                if ((xTaskGetTickCount() - last_vol_activity_tick) > pdMS_TO_TICKS(VOL_TIMEOUT_MS)) {
                    current_state = (is_input_sig_flag == 1) ? STATE_SPECTRUM : STATE_IDLE_CAT2;
                }
                vTaskDelay(pdMS_TO_TICKS(50));
                break;

            case STATE_SETUP_MENU:
                menu_update();
                vTaskDelay(pdMS_TO_TICKS(40));
                break;

            default:
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
        }
    }
}

// ==================== MAIN ====================
void app_main(void) {
    ESP_LOGI(TAG, "=== СТАРТ СИСТЕМИ CATZILLA ===");

    // Конфігурація ліній живлення
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_EN_ALL_POWER) | (1ULL << PIN_VSYS_EN) |
                        (1ULL << PIN_BLE_EN) | (1ULL << PIN_EN_POW_ADAU) |
                        (1ULL << PIN_EN_ADDR_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    gpio_set_level(PIN_EN_ALL_POWER, 1);
    gpio_set_level(PIN_VSYS_EN, 1);
    gpio_set_level(PIN_BLE_EN, 1);
    gpio_set_level(PIN_EN_POW_ADAU, 1);
    gpio_set_level(PIN_EN_ADDR_LED, 1);

    vTaskDelay(pdMS_TO_TICKS(100));

    // Ініціалізація черг та периферії
    g_fft_process_result_queue = xQueueCreate(5, COLUM_SIZE * sizeof(uint8_t));

    lcd_bus_init();
    lcd_init();
    analizator_init();

    // Включає і кнопки, і світлодіоди
    buttons_init();

    // Запуск задач
    xTaskCreate(temperature_task, "temperature_task", 4096, NULL, 5, NULL);
    xTaskCreate(ui_display_task, "ui_display_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Система успішно запущена!");
}