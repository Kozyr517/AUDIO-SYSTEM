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
#include "i2c_bus.h"
#include "lcd.h"
#include "analizator.h"
#include "eeprom_24lc128.h"
#include "screens.h" 
#include "animation.h"
#include "audio_manager.h"

static const char *TAG = "MAIN";

// Прототипи функцій
void sleep_timer_cb(TimerHandle_t xTimer);
void check_system_idle(void);

// ==================== ПІНИ ТА КОНФІГУРАЦІЯ ====================
#define PIN_VSYS_EN         GPIO_NUM_4
#define PIN_BLE_EN          GPIO_NUM_5
#define PIN_EN_POW_ADAU     GPIO_NUM_10
#define PIN_EN_ALL_POWER    GPIO_NUM_21
#define PIN_EN_ADDR_LED     GPIO_NUM_47
#define PIN_ADAU_RES        GPIO_NUM_3
#define PIN_GP9             GPIO_NUM_9

#define VOL_TIMEOUT_MS      3000
#define COLUM_SIZE          254

// ==================== ІНІЦІАЛІЗАЦІЯ СТАНУ ====================
volatile app_state_t current_state = STATE_BOOT;
uint8_t master_volume = 0; 
TickType_t last_vol_activity_tick = 0;

// ==================== ГЛОБАЛЬНІ ЗМІННІ ТА ПРАПОРЦІ ====================
extern volatile uint8_t is_input_sig_flag;    
extern volatile uint8_t button_idle_flag;     

QueueHandle_t g_fft_process_result_queue = NULL;
static TimerHandle_t sleep_timer = NULL;     
static volatile bool sleep_timer_running = false;

// ==================== УПРАВЛІННЯ ЖИВЛЕННЯМ ПЕРИФЕРІЇ ====================

static void power_on_peripherals(void) {
    gpio_set_level(PIN_EN_ALL_POWER, 1);
    vTaskDelay(pdMS_TO_TICKS(50)); 

    gpio_set_level(PIN_VSYS_EN, 1);
    vTaskDelay(pdMS_TO_TICKS(50)); 

    gpio_set_level(PIN_EN_POW_ADAU, 1);
    gpio_set_level(PIN_GP9, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    gpio_set_level(PIN_BLE_EN, 1);
    gpio_set_level(PIN_EN_ADDR_LED, 1);
    
    gpio_set_level(PIN_ADAU_RES, 1); 
    vTaskDelay(pdMS_TO_TICKS(150)); 
}

static void power_off_peripherals(void) {
    gpio_set_level(PIN_EN_ALL_POWER, 0);
    gpio_set_level(PIN_VSYS_EN, 0);
    gpio_set_level(PIN_EN_POW_ADAU, 0);
    gpio_set_level(PIN_GP9, 0);
    gpio_set_level(PIN_BLE_EN, 0);
    gpio_set_level(PIN_EN_ADDR_LED, 0);
    gpio_set_level(PIN_ADAU_RES, 0); 
}

// Колбек таймера 10-хвилинної бездіяльності
void sleep_timer_cb(TimerHandle_t xTimer) {
    current_state = STATE_SLEEP_SHUTDOWN;
}

// Процедура вимкнення та входу в Light Sleep
static void execute_sleep_sequence(void) {
    ESP_LOGI(TAG, "5 секунд минуло. Відтворення анімації вимкнення...");
    
    TickType_t start_tick = xTaskGetTickCount();
    const TickType_t anim_duration = pdMS_TO_TICKS(3000); 

    while ((xTaskGetTickCount() - start_tick) < anim_duration) {
        lcd_clear();
        animation_draw(ANIM_CAT3, 63, 8);
        lcd_update();
        vTaskDelay(pdMS_TO_TICKS(40)); 
    }

    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG, "Очікування відпускання кнопок...");
    gpio_num_t btn_pins[] = {PIN_BTN_1, PIN_BTN_2, PIN_BTN_3, PIN_BTN_4};
    bool any_pressed = true;
    while (any_pressed) {
        any_pressed = false;
        for (int i = 0; i < 4; i++) {
            if (gpio_get_level(btn_pins[i]) == 1) { 
                any_pressed = true;
                break;
            }
        }
        if (any_pressed) vTaskDelay(pdMS_TO_TICKS(50));
    }
    vTaskDelay(pdMS_TO_TICKS(150)); 

    ESP_LOGW(TAG, "Знеструмлення периферії...");
    power_off_peripherals();

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

    // === ПРОБУДЖЕННЯ ===
    ESP_LOGI(TAG, "Пробудження з Light Sleep. Відновлення роботи...");

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
    for (size_t i = 0; i < 4; i++) {
        gpio_wakeup_disable(btn_pins[i]);
    }

    any_pressed = true;
    while (any_pressed) {
        any_pressed = false;
        for (int i = 0; i < 4; i++) {
            if (gpio_get_level(btn_pins[i]) == 1) {
                any_pressed = true;
                break;
            }
        }
        if (any_pressed) vTaskDelay(pdMS_TO_TICKS(20)); 
    }
    vTaskDelay(pdMS_TO_TICKS(50)); 

    power_on_peripherals();
    vTaskDelay(pdMS_TO_TICKS(800)); 

    // TODO: apply_all_settings()
    
    lcd_init();
    draw_boot_animation(); 
    analizator_init();

    current_state = (is_input_sig_flag == 1) ? STATE_SPECTRUM : STATE_IDLE_CAT2;
}

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

// ==================== ЗАДАЧА ДИСПЛЕЯ (UI STATE MACHINE) ====================

static void ui_display_task(void *pvParameters) {
    current_state = STATE_BOOT;
    draw_boot_animation();

    current_state = (is_input_sig_flag == 1) ? STATE_SPECTRUM : STATE_IDLE_CAT2;

    while (1) {
        switch (current_state) {
            case STATE_IDLE_CAT2:
                extern volatile bool is_muted;
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
                extern volatile bool is_muted;
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
                draw_volume_popup(master_volume);
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

    gpio_deep_sleep_hold_dis();

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

    power_on_peripherals();

    sleep_timer = xTimerCreate("SleepTimer", pdMS_TO_TICKS(600000), pdFALSE, NULL, sleep_timer_cb);
    g_fft_process_result_queue = xQueueCreate(5, COLUM_SIZE * sizeof(uint8_t));

    vTaskDelay(pdMS_TO_TICKS(800)); 

    lcd_bus_init();
    lcd_init();
    buttons_init();
    i2c_bus_init();

    vTaskDelay(pdMS_TO_TICKS(50));

    eeprom_init(i2c_bus_handle);
    eeprom_load_settings(&g_settings);
    
    master_volume = g_settings.adau_main_volume;
    
    // TODO: apply_all_settings()
    audio_manager_init_all();

    analizator_init();
    check_system_idle();

    xTaskCreate(temperature_task, "temperature_task", 4096, NULL, 3, NULL);
    xTaskCreate(ui_display_task, "ui_display_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Система успішно запущена!");
}