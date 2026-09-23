#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "freertos/portmacro.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"

#include "app_state.h"
#include "cooling.h"
#include "buttons.h"
#include "menu.h"
#include "lcd.h"
#include "analizator.h"
#include "animation.h"
#include "Sinclair_S8x8.h"

static const char *TAG = "MAIN";

// Прототипи функцій
void sleep_timer_cb(TimerHandle_t xTimer);
void check_system_idle(void);
void draw_boot_animation(void);

// ==================== ПІНИ ТА КОНФІГУРАЦІЯ ====================
#define PIN_VSYS_EN         GPIO_NUM_4
#define PIN_BLE_EN          GPIO_NUM_5
#define PIN_EN_POW_ADAU     GPIO_NUM_10
#define PIN_EN_ALL_POWER    GPIO_NUM_21
#define PIN_EN_ADDR_LED     GPIO_NUM_47
#define PIN_ADAU_RES        GPIO_NUM_3
#define PIN_GP9            GPIO_NUM_9

#define VOL_TIMEOUT_MS      3000
#define COLUM_SIZE          254
#define COLUM_FRAME_SKIP    25 

// ==================== ІНІЦІАЛІЗАЦІЯ СТАНУ ====================
volatile app_state_t current_state = STATE_BOOT;
uint8_t master_volume = 10;
TickType_t last_vol_activity_tick = 0;

// ==================== ГЛОБАЛЬНІ ЗМІННІ ТА ПРАПОРЦІ ====================
extern TypeDef_GP1247AI lcd;
extern volatile uint8_t is_input_sig_flag;    // Прапорець аудіосигналу (0 = немає, 1 = є)
extern volatile uint8_t button_idle_flag;     // Прапорець активності кнопок (0 = спокій, 1 = натиснута)

QueueHandle_t g_fft_process_result_queue = NULL;
static TimerHandle_t sleep_timer = NULL;     // 10-хвилинний таймер бездіяльності

static uint8_t colum_data[COLUM_SIZE] = {0};
static uint8_t old_colum[COLUM_SIZE] = {0};
static uint8_t colum_peak_pos[COLUM_SIZE] = {0};
static uint8_t colum_timers[COLUM_SIZE] = {0};

// Прапорець для безпечного трекінгу таймера
static volatile bool sleep_timer_running = false;


// ==================== УПРАВЛІННЯ ЖИВЛЕННЯМ ПЕРИФЕРІЇ ====================

static void power_on_peripherals(void) {
    gpio_set_level(PIN_EN_ALL_POWER, 1);
    vTaskDelay(pdMS_TO_TICKS(20)); // Пауза на заряд первинних ємностей

    gpio_set_level(PIN_VSYS_EN, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    gpio_set_level(PIN_EN_POW_ADAU, 1);
    gpio_set_level(PIN_GP9, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    gpio_set_level(PIN_BLE_EN, 1);
    gpio_set_level(PIN_EN_ADDR_LED, 1);
    
    // Встановлюємо 1 (Active High), щоб зняти ADAU з режиму Reset
    gpio_set_level(PIN_ADAU_RES, 1); 

    // Час для стабілізації живлення
    vTaskDelay(pdMS_TO_TICKS(100));
}

static void power_off_peripherals(void) {
    gpio_set_level(PIN_EN_ALL_POWER, 0);
    gpio_set_level(PIN_VSYS_EN, 0);
    gpio_set_level(PIN_EN_POW_ADAU, 0);
    gpio_set_level(PIN_GP9, 0);
    gpio_set_level(PIN_BLE_EN, 0);
    gpio_set_level(PIN_EN_ADDR_LED, 0);
    gpio_set_level(PIN_ADAU_RES, 0); // ADAU утримаємо в Reset
}


// Колбек таймера 10-хвилинної бездіяльності
void sleep_timer_cb(TimerHandle_t xTimer) {
    current_state = STATE_SLEEP_SHUTDOWN;
}

// Процедура вимкнення та входу в Light Sleep
static void execute_sleep_sequence(void) {
    ESP_LOGI(TAG, "5 секунд минуло. Відтворення анімації вимкнення...");
    
    // === ЗАМІНІТЬ ЦЕЙ БЛОК ===
    TickType_t start_tick = xTaskGetTickCount();
    const TickType_t anim_duration = pdMS_TO_TICKS(3000); // 3 секунди анімації

    while ((xTaskGetTickCount() - start_tick) < anim_duration) {
        lcd_clear();
        animation_draw(ANIM_CAT3, 63, 8);
        lcd_update();
        vTaskDelay(pdMS_TO_TICKS(40)); // ~25 FPS
    }

    // 2. Даємо час на відтворення анімації
    vTaskDelay(pdMS_TO_TICKS(3000));

    // 3. Очікування відпускання кнопок перед сном
    ESP_LOGI(TAG, "Очікування відпускання кнопок...");
    gpio_num_t btn_pins[] = {PIN_BTN_1, PIN_BTN_2, PIN_BTN_3, PIN_BTN_4};
    bool any_pressed = true;
    while (any_pressed) {
        any_pressed = false;
        for (int i = 0; i < 4; i++) {
            if (gpio_get_level(btn_pins[i]) == 1) { // 1 = натиснута
                any_pressed = true;
                break;
            }
        }
        if (any_pressed) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
    vTaskDelay(pdMS_TO_TICKS(150)); // Дебаунс

    // 4. Знеструмлюємо периферію
    ESP_LOGW(TAG, "Знеструмлення периферії...");
    power_off_peripherals();

    // 5. Конфігурація пінів для пробудження (HIGH LEVEL)
    for (size_t i = 0; i < 4; i++) {
        gpio_set_direction(btn_pins[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode(btn_pins[i], GPIO_PULLDOWN_ONLY);
        gpio_sleep_set_direction(btn_pins[i], GPIO_MODE_INPUT);
        gpio_sleep_set_pull_mode(btn_pins[i], GPIO_PULLDOWN_ONLY);
        gpio_wakeup_enable(btn_pins[i], GPIO_INTR_HIGH_LEVEL);
    }
    
    esp_sleep_enable_gpio_wakeup();

    ESP_LOGI(TAG, "Вхід у Light Sleep. Процесор зупинено.");
    esp_light_sleep_start();

    // =================================================================
    // === ПРОБУДЖЕННЯ (Режим без рестарту) ===
    // =================================================================
    
    ESP_LOGI(TAG, "Пробудження з Light Sleep. Відновлення роботи...");

    // 1. МИТТЄВО вимикаємо wakeup-переривання
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
    for (size_t i = 0; i < 4; i++) {
        gpio_wakeup_disable(btn_pins[i]);
    }

    // 2. Очікуємо, поки користувач ВІДПУСТИТЬ кнопку
    any_pressed = true;
    while (any_pressed) {
        any_pressed = false;
        for (int i = 0; i < 4; i++) {
            if (gpio_get_level(btn_pins[i]) == 1) {
                any_pressed = true;
                break;
            }
        }
        if (any_pressed) {
            vTaskDelay(pdMS_TO_TICKS(20)); 
        }
    }
    vTaskDelay(pdMS_TO_TICKS(50)); // Дебаунс після відпускання

    // 3. Повертаємо живлення периферії
    power_on_peripherals();

    // 4. ВІДНОВЛЮЄМО нормальну роботу драйвера кнопок
    buttons_init();

    // 5. Відновлення шини дисплея
    lcd_bus_init(); 
    lcd_init();
    vTaskDelay(pdMS_TO_TICKS(120));

    // 6. Першою запускаємо анімацію запуску
    draw_boot_animation();

    // 7. Відновлюємо аналізатор
    analizator_init();

    // 8. Повертаємось у стандартний цикл
    current_state = (is_input_sig_flag == 1) ? STATE_SPECTRUM : STATE_IDLE_CAT2;
}

// Безпечна функція перевірки стану бездіяльності
void check_system_idle(void) {
    if (sleep_timer == NULL) return;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    bool is_in_isr = xPortInIsrContext();

    if (is_input_sig_flag == 0 && button_idle_flag == 0) {
        if (!sleep_timer_running) {
            sleep_timer_running = true;
            if (is_in_isr) {
                xTimerStartFromISR(sleep_timer, &xHigherPriorityTaskWoken);
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            } else {
                xTimerStart(sleep_timer, 0);
            }
        }
    } 
    else {
        if (sleep_timer_running) {
            sleep_timer_running = false;
            if (is_in_isr) {
                xTimerStopFromISR(sleep_timer, &xHigherPriorityTaskWoken);
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            } else {
                xTimerStop(sleep_timer, 0);
            }
        }
    }
}


// ==================== ГРАФІКА ТА ІНТЕРФЕЙС ====================

void draw_boot_animation(void) {
    for (int16_t x = -130; x <= 253; x += 6) {
        lcd_clear();
        animation_draw(ANIM_CAT1, x, 8);
        lcd_update();
        vTaskDelay(pdMS_TO_TICKS(40)); // Затримка для плавної анімації
    }
}

void draw_idle_cat2_frame(void) {
    lcd_clear();
    animation_draw(ANIM_CAT2, 63, 8);
    lcd_update();
}

// Кадр для стану MUTE: котик ANIM_CAT2 + напис "MUTE" у правому верхньому кутку
void draw_mute_frame(void) {
    lcd_clear();
    animation_draw(ANIM_CAT2, 63, 8);
    // x = 218 залишає відступ у 4 пікселі від правого краю (254 - 32px = 222)
    lcd_print("MUTE", 218, 2, (const uint8_t*)Sinclair_S8x8, 0);
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


// ==================== ЗАДАЧА ДИСПЛЕЯ (UI STATE MACHINE) ====================

static void ui_display_task(void *pvParameters) {
    current_state = STATE_BOOT;
    draw_boot_animation();

    current_state = (is_input_sig_flag == 1) ? STATE_SPECTRUM : STATE_IDLE_CAT2;

    while (1) {
        switch (current_state) {
            case STATE_IDLE_CAT2:
                if (is_muted) {
                    draw_mute_frame();
                    vTaskDelay(pdMS_TO_TICKS(60));
                } else if (is_input_sig_flag == 1) {
                    current_state = STATE_SPECTRUM;
                    check_system_idle();
                } else {
                    draw_idle_cat2_frame();
                    vTaskDelay(pdMS_TO_TICKS(60));
                }
                break;

            case STATE_SPECTRUM:
                if (is_muted) {
                    draw_mute_frame();
                    vTaskDelay(pdMS_TO_TICKS(60));
                } else if (is_input_sig_flag == 0) {
                    current_state = STATE_IDLE_CAT2;
                    check_system_idle();
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

            case STATE_SLEEP_SHUTDOWN:
                execute_sleep_sequence();
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

    // Знімаємо фіксацію пінів після пробудження
    gpio_deep_sleep_hold_dis();

    // Конфігурація ліній живлення та сигналів керування
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_EN_ALL_POWER) | (1ULL << PIN_VSYS_EN) |
                        (1ULL << PIN_BLE_EN) | (1ULL << PIN_EN_POW_ADAU) |
                        (1ULL << PIN_EN_ADDR_LED) | (1ULL << PIN_ADAU_RES) |
                        (1ULL << PIN_GP9),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Увімкнення живлення
    power_on_peripherals();

    // Створення 10-хвилинного таймера сну (600 000 мс)
    sleep_timer = xTimerCreate("SleepTimer", pdMS_TO_TICKS(600000), pdFALSE, NULL, sleep_timer_cb);

    // Ініціалізація черг та дисплея
    g_fft_process_result_queue = xQueueCreate(5, COLUM_SIZE * sizeof(uint8_t));

    lcd_bus_init();
    lcd_init();

    vTaskDelay(pdMS_TO_TICKS(120)); // Чекаємо готовності матриці

    // Ініціалізуємо кнопки
    buttons_init();

    // Запуск аналізатора
    analizator_init();

    // Первинна перевірка стану активності для таймера сну
    check_system_idle();

    // Запуск задач
    xTaskCreate(temperature_task, "temperature_task", 4096, NULL, 3, NULL);
    xTaskCreate(ui_display_task, "ui_display_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Система успішно запущена!");
}