#include "ui_app.h"
#include "screens.h"
#include "animation.h" 
#include "analizator.h"
#include "menu.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Конфігурація кнопок за апаратними пінами:
#define BTN_VOL_UP_PIN   GPIO_NUM_1   // Кнопка 1: Гучність Вгору (+)
#define BTN_VOL_DOWN_PIN GPIO_NUM_2   // Кнопка 2: Гучність Вниз (-)
#define BTN_MENU_PIN     GPIO_NUM_41  // Кнопка 3: Довге натискання (~2 сек) -> Виклик/Вихід з меню

#define VOL_STEP         5
#define VOL_MAX          100
#define VOL_MIN          0

static ui_state_t current_state = UI_STATE_BOOT;
static uint8_t current_volume = 50;

static bool show_volume_overlay = false;
static bool eeprom_save_pending = false;
static TickType_t last_volume_change_time = 0;

static uint8_t load_volume_from_eeprom(void) {
    // ТИМЧАСОВО ВИМКНЕНО I2C ДЛЯ ТЕСТІВ UI
    return 50; // Дефолтне значення
}

static void save_volume_to_eeprom(uint8_t vol) {
    // ТИМЧАСОВО ВИМКНЕНО I2C ДЛЯ ТЕСТІВ UI
}

static void button_task(void *pvParameters) {
    for (;;) {
        if (current_state != UI_STATE_BOOT) {
            
            // 1. КНОПКА 1 (ВГОРУ): Миттєве збільшення гучності
            if (gpio_get_level(BTN_VOL_UP_PIN) == 0) {
                if (current_volume + VOL_STEP <= VOL_MAX) current_volume += VOL_STEP;
                else current_volume = VOL_MAX;
                
                show_volume_overlay = true;
                eeprom_save_pending = true;
                last_volume_change_time = xTaskGetTickCount();
                vTaskDelay(pdMS_TO_TICKS(150)); // Антидребезг
            }

            // 2. КНОПКА 2 (ВНИЗ): Миттєве зменшення гучності
            if (gpio_get_level(BTN_VOL_DOWN_PIN) == 0) {
                if (current_volume >= VOL_STEP) current_volume -= VOL_STEP;
                else current_volume = VOL_MIN;

                show_volume_overlay = true;
                eeprom_save_pending = true;
                last_volume_change_time = xTaskGetTickCount();
                vTaskDelay(pdMS_TO_TICKS(150)); // Антидребезг
            }

            // 3. КНОПКА 3 (МЕНЮ): Довге затискання (2 секунди) для входу/виходу з меню
            if (gpio_get_level(BTN_MENU_PIN) == 0) {
                TickType_t press_start = xTaskGetTickCount();
                bool long_press_triggered = false;

                while (gpio_get_level(BTN_MENU_PIN) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(50));
                    
                    // Якщо затиснули більше ніж на 2 секунди (2000 мс)
                    if (!long_press_triggered && (xTaskGetTickCount() - press_start > pdMS_TO_TICKS(2000))) {
                        long_press_triggered = true;
                        show_volume_overlay = false; // Ховаємо шкалу гучності при переході в меню
                        
                        // Перемикаємо стани: з аналізатора в меню, або з меню назад в аналізатор
                        if (current_state == UI_STATE_MENU) {
                            current_state = UI_STATE_ANALYZER;
                        } else {
                            current_state = UI_STATE_MENU;
                        }
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20)); // Опитування кнопок кожні 20 мс
    }
}

static void ui_update_task(void *pvParameters) {
    // ЕТАП 1: ЗАВАНТАЖЕННЯ ТА СТАБІЛІЗАЦІЯ
    if (current_state == UI_STATE_BOOT) {
        show_startup_animation();
        vTaskDelay(pdMS_TO_TICKS(1500)); 
        
        current_volume = load_volume_from_eeprom();
        current_state = UI_STATE_ANALYZER;
    }

    // ЕТАП 2: ГОЛОВНИЙ ЦИКЛ ЕКРАНА
    for (;;) {
        // Перевірка таймауту відображення гучності та збереження в EEPROM (через 2 сек)
        if (xTaskGetTickCount() - last_volume_change_time > pdMS_TO_TICKS(2000)) {
            if (show_volume_overlay) show_volume_overlay = false;
            
            if (eeprom_save_pending) {
                save_volume_to_eeprom(current_volume);
                eeprom_save_pending = false;
            }
        }

        // Пріоритет візуалізації
        if (show_volume_overlay) {
            show_catzilla_screen(current_volume); // Оверлей шкали лапок гучності
        } else {
            switch (current_state) {
                case UI_STATE_ANALYZER:
                    if (is_input_sig_flag) {
                        show_spectr(); // Спектроаналізатор, якщо є сигнал
                    } else {
                        animation_draw(); // Малювання вибраної анімації (кіт), якщо тиша
                    }
                    break;
                case UI_STATE_MENU:
                    menu_update(); // Екран та оновлення меню налаштувань
                    break;
                default:
                    break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(30)); // Частота оновлення екрана ~33 кадрів/с
    }
}

void ui_app_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BTN_VOL_UP_PIN) | (1ULL << BTN_VOL_DOWN_PIN) | (1ULL << BTN_MENU_PIN),
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