#include "ui_app.h"
#include "screens.h"
#include "idle_anim.h"  // Модуль анімацій кота замість синусу
#include "analizator.h"
#include "dsp.h"
#include "i2c_bus.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BTN_OK_PIN      GPIO_NUM_0
#define BTN_UP_PIN      GPIO_NUM_4
#define BTN_DOWN_PIN    GPIO_NUM_5

#define VOL_STEP        5
#define VOL_MAX         100
#define VOL_MIN         0

static ui_state_t current_state = UI_STATE_BOOT;
static uint8_t current_volume = 50;

static bool show_volume_overlay = false;
static bool eeprom_save_pending = false;
static TickType_t last_volume_change_time = 0;

static uint8_t load_volume_from_eeprom(void) {
    uint8_t mem_addr[2] = { 
        (uint8_t)(EEPROM_VOL_MASTER >> 8), 
        (uint8_t)(EEPROM_VOL_MASTER & 0xFF) 
    };
    uint8_t vol = 50;

    esp_err_t err = i2c_master_write_read_device(
        I2C_MASTER_NUM, EEPROM_I2C_ADDR, 
        mem_addr, 2, &vol, 1, pdMS_TO_TICKS(100)
    );

    if (err != ESP_OK || vol > VOL_MAX) {
        vol = 50;
    }
    return vol;
}

static void save_volume_to_eeprom(uint8_t vol) {
    uint8_t data[3] = { 
        (uint8_t)(EEPROM_VOL_MASTER >> 8), 
        (uint8_t)(EEPROM_VOL_MASTER & 0xFF), 
        vol 
    };

    i2c_master_write_to_device(
        I2C_MASTER_NUM, EEPROM_I2C_ADDR, 
        data, sizeof(data), pdMS_TO_TICKS(100)
    );

    vTaskDelay(pdMS_TO_TICKS(10));
}

static void button_task(void *pvParameters) {
    for (;;) {
        if (current_state != UI_STATE_BOOT) {
            if (gpio_get_level(BTN_UP_PIN) == 0) {
                if (current_volume + VOL_STEP <= VOL_MAX) current_volume += VOL_STEP;
                else current_volume = VOL_MAX;
                
                dsp_set_volume(current_volume);
                show_volume_overlay = true;
                eeprom_save_pending = true;
                last_volume_change_time = xTaskGetTickCount();
                vTaskDelay(pdMS_TO_TICKS(150));
            }

            if (gpio_get_level(BTN_DOWN_PIN) == 0) {
                if (current_volume >= VOL_STEP) current_volume -= VOL_STEP;
                else current_volume = VOL_MIN;

                dsp_set_volume(current_volume);
                show_volume_overlay = true;
                eeprom_save_pending = true;
                last_volume_change_time = xTaskGetTickCount();
                vTaskDelay(pdMS_TO_TICKS(150));
            }

            if (gpio_get_level(BTN_OK_PIN) == 0) {
                TickType_t press_start = xTaskGetTickCount();
                bool long_press = false;

                while (gpio_get_level(BTN_OK_PIN) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(50));
                    if (!long_press && (xTaskGetTickCount() - press_start > pdMS_TO_TICKS(1000))) {
                        long_press = true;
                        show_volume_overlay = false;
                        current_state = (current_state == UI_STATE_MENU) ? UI_STATE_ANALYZER : UI_STATE_MENU;
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void ui_update_task(void *pvParameters) {
    // 1. СТАДІЯ ЗАВАНТАЖЕННЯ: анімація та очікування стабілізації
    if (current_state == UI_STATE_BOOT) {
        show_startup_animation();
        
        // Пауза 1.5 сек для стабілізації напруги
        vTaskDelay(pdMS_TO_TICKS(1500)); 
        
        // Зчитування EEPROM після стабілізації
        current_volume = load_volume_from_eeprom();
        dsp_set_volume(current_volume);
        
        current_state = UI_STATE_ANALYZER;
    }

    // 2. ОСНОВНИЙ РОБОЧИЙ ЦИКЛ
    for (;;) {
        if (xTaskGetTickCount() - last_volume_change_time > pdMS_TO_TICKS(2000)) {
            if (show_volume_overlay) show_volume_overlay = false;
            
            if (eeprom_save_pending) {
                save_volume_to_eeprom(current_volume);
                eeprom_save_pending = false;
            }
        }

        if (show_volume_overlay) {
            show_catzilla_screen(current_volume);
        } else {
            switch (current_state) {
                case UI_STATE_ANALYZER:
                    if (is_input_sig_flag) {
                        show_spectr();
                    } else {
                        show_idle_animation(); // Виклик анімації з idle_anim.c
                    }
                    break;
                case UI_STATE_MENU:
                    show_param_menu();
                    break;
                default:
                    break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

void ui_app_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BTN_OK_PIN) | (1ULL << BTN_UP_PIN) | (1ULL << BTN_DOWN_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    xTaskCreate(ui_update_task, "ui_update_task", 4096, NULL, 3, NULL);
    xTaskCreate(button_task, "button_task", 2048, NULL, 2, NULL);
}

void ui_set_state(ui_state_t new_state) { current_state = new_state; }
ui_state_t ui_get_state(void) { return current_state; }
uint8_t ui_get_volume(void) { return current_volume; }