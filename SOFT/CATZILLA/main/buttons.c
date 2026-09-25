#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "led_strip.h"
#include "esp_log.h"
#include "esp_err.h"

// Власні заголовки
#include "menu.h"
#include "app_state.h"
#include "buttons.h"

#define MYSPACE_TARGET_COUNT 12
float myspace_values[MYSPACE_TARGET_COUNT] = {
    1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, // Distances (FL, FR, C, SUB, SL, SR)
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f  // Gains/Volume (FL, FR, C, SUB, SL, SR)
};

static const char *TAG_BTN = "BUTTONS";

// Зовнішні залежності системи
extern volatile uint8_t is_input_sig_flag;
extern void check_system_idle(void);
extern void sleep_timer_cb(TimerHandle_t xTimer);

// Прапорець активності кнопок та таймер скидання
volatile uint8_t button_idle_flag = 0;
static TimerHandle_t button_activity_timer = NULL;

#define MENU_AUTO_EXIT_TIMEOUT_MS 20000 
static TickType_t last_menu_activity_tick = 0;

// ==================== ЛОГІКА MUTE ====================
volatile bool is_muted = false;

void set_mute_state(bool enable_mute) {
    is_muted = enable_mute;
    if (is_muted) {
        // ESP_LOGW(TAG_BTN, "[AUDIO] MUTE УВІМКНЕНО (Звук вимкнено)");
        // TODO: I2C команда вимкнення звуку на ADAU1452 або реле
    } else {
        // ESP_LOGI(TAG_BTN, "[AUDIO] UNMUTE (Звук відновлено)");
        // TODO: I2C команда увімкнення звуку на ADAU1452 або реле
    }
}

void toggle_mute(void) {
    set_mute_state(!is_muted);
}

// ==================== ТАЙМЕРИ АКТИВНОСТІ ====================
static void button_activity_timer_cb(TimerHandle_t xTimer) {
    button_idle_flag = 0;
    check_system_idle();
}

static void mark_button_activity(void) {
    button_idle_flag = 1;
    check_system_idle();

    if (button_activity_timer != NULL) {
        if (xPortInIsrContext()) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xTimerResetFromISR(button_activity_timer, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        } else {
            xTimerReset(button_activity_timer, 0);
        }
    }
}

static void load_menu_pointers_from_ram(void) {
    eq_menu_pointer        = saved_eq_preset;
    filters_menu_pointer   = saved_filter;
    phono_menu_pointer     = 0;
    
    // Скидання трирівневого меню MySpace
    myspace_depth           = 0;
    myspace_speaker_pointer = 0;
    myspace_param_pointer   = 0;
}

static void exit_setup_menu(void) {
    myspace_depth = 0;
    current_state = (is_input_sig_flag == 1) ? STATE_SPECTRUM : STATE_IDLE_CAT2;
    menu_pointer = MAIN_MENU_NUM;
    old_menu_pointer = 255;
}

// ==================== ДАНІ ТА СТРУКТУРИ LED ====================
typedef struct { float r, g, b; } Color;
static const Color COLOR_IDLE   = {4.0f,  24.0f, 9.0f};  
static const Color COLOR_ACTIVE = {25.5f, 18.0f, 0.0f};

typedef struct {
    gpio_num_t gpio;
    int led_index;
} ButtonMapping;

static const ButtonMapping btn_map[LED_COUNT_FRONT] = {
    { PIN_BTN_1, 3 }, { PIN_BTN_2, 2 }, { PIN_BTN_3, 1 }, { PIN_BTN_4, 0 }
};

static led_strip_handle_t strip_front = NULL;
static Color current_colors[LED_COUNT_FRONT];
static Color target_colors[LED_COUNT_FRONT];
static QueueHandle_t gpio_event_queue = NULL;

static void led_fader_task(void *pvParameters) {
    const float step = 0.25f; 
    while (1) {
        for (int i = 0; i < LED_COUNT_FRONT; i++) {
            int led_idx = btn_map[i].led_index;
            if (gpio_get_level(btn_map[i].gpio) == 1) {
                target_colors[led_idx] = COLOR_ACTIVE;
            } else {
                target_colors[led_idx] = COLOR_IDLE;
            }
        }

        for (int i = 0; i < LED_COUNT_FRONT; i++) {
            current_colors[i].r += (target_colors[i].r - current_colors[i].r) * step;
            current_colors[i].g += (target_colors[i].g - current_colors[i].g) * step;
            current_colors[i].b += (target_colors[i].b - current_colors[i].b) * step;

            if (strip_front) {
                led_strip_set_pixel(strip_front, i, 
                                    (uint8_t)current_colors[i].r, 
                                    (uint8_t)current_colors[i].g, 
                                    (uint8_t)current_colors[i].b);
            }
        }
        
        if (strip_front) led_strip_refresh(strip_front);
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

static void leds_init(void) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = PIN_ADDR_L_FRONT,
        .max_leds = LED_COUNT_FRONT,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &strip_front));

    for (int i = 0; i < LED_COUNT_FRONT; i++) {
        current_colors[i] = COLOR_IDLE;
        target_colors[i]  = COLOR_IDLE;
    }

    xTaskCreate(led_fader_task, "led_fader", 2048, NULL, 5, NULL);
}

// ==================== РЕГУЛЮВАННЯ ГУЧНОСТІ ====================
static void change_volume(int delta) {
    int new_vol = (int)master_volume + delta;
    
    if (new_vol > 100) new_vol = 100;
    if (new_vol < 0)   new_vol = 0;
    
    if (new_vol != master_volume) {
        master_volume = (uint8_t)new_vol;
        // ESP_LOGI(TAG_BTN, "[VOLUME] New Level: %d | Sent Command to DSP", master_volume);
    }
    
    current_state = STATE_VOLUME_POPUP;
    last_vol_activity_tick = xTaskGetTickCount();
}

// ==================== ОПРЕОБКА КНОПОК MYSPACE ====================
static void process_myspace_buttons(uint32_t butt_num) {
    switch (butt_num) {
        case PIN_BTN_1: // Вгору / Збільшення значення
            if (myspace_depth == 2) {
                if (myspace_param_pointer == 0) { // Дистанція (DIST)
                    myspace_values[myspace_speaker_pointer] += 0.10f;
                    if (myspace_values[myspace_speaker_pointer] > 10.0f) {
                        myspace_values[myspace_speaker_pointer] = 10.0f;
                    }
                    myspace_values[myspace_speaker_pointer] = roundf(myspace_values[myspace_speaker_pointer] * 10.0f) / 10.0f;
                } else { // Гучність (GAIN)
                    myspace_values[myspace_speaker_pointer + 6] += 0.50f;
                    if (myspace_values[myspace_speaker_pointer + 6] > 12.0f) {
                        myspace_values[myspace_speaker_pointer + 6] = 12.0f;
                    }
                    myspace_values[myspace_speaker_pointer + 6] = roundf(myspace_values[myspace_speaker_pointer + 6] * 2.0f) / 2.0f;
                }
            } else if (myspace_depth == 1) {
                // Перемикаємо на GAIN (1), оскільки він візуально зверху
                myspace_param_pointer = 1;
            } else if (myspace_depth == 0) {
                if (myspace_speaker_pointer > 0) myspace_speaker_pointer--;
            }
            break;

        case PIN_BTN_2: // Вниз / Зменшення значення
            if (myspace_depth == 2) {
                if (myspace_param_pointer == 0) { // Дистанція (DIST)
                    myspace_values[myspace_speaker_pointer] -= 0.10f;
                    if (myspace_values[myspace_speaker_pointer] < 0.0f) {
                        myspace_values[myspace_speaker_pointer] = 0.0f;
                    }
                    myspace_values[myspace_speaker_pointer] = roundf(myspace_values[myspace_speaker_pointer] * 10.0f) / 10.0f;
                } else { // Гучність (GAIN)
                    myspace_values[myspace_speaker_pointer + 6] -= 0.50f;
                    if (myspace_values[myspace_speaker_pointer + 6] < -12.0f) {
                        myspace_values[myspace_speaker_pointer + 6] = -12.0f;
                    }
                    myspace_values[myspace_speaker_pointer + 6] = roundf(myspace_values[myspace_speaker_pointer + 6] * 2.0f) / 2.0f;
                }
            } else if (myspace_depth == 1) {
                // Перемикаємо на DIST (0), оскільки він візуально знизу
                myspace_param_pointer = 0;
            } else if (myspace_depth == 0) {
                if (myspace_speaker_pointer < 5) myspace_speaker_pointer++;
            }
            break;

        case PIN_BTN_3: // Вибір / Підтвердження (OK)
            if (myspace_depth == 0) {
                myspace_depth = 1;
                myspace_param_pointer = 1; // За замовчуванням стаємо на верхній параметр (GAIN)
            } else if (myspace_depth == 1) {
                myspace_depth = 2;
            } else if (myspace_depth == 2) {
                myspace_depth = 1; // Завершення редагування та повернення до вибору параметрів
            }
            // ESP_LOGI(TAG_BTN, "[MYSPACE] Speaker: %d | Param: %d | Depth: %d", 
            //          myspace_speaker_pointer, myspace_param_pointer, myspace_depth);
            break;

        case PIN_BTN_4: // Назад / Вихід
            if (myspace_depth == 2) {
                myspace_depth = 1; 
            } else if (myspace_depth == 1) {
                myspace_depth = 0; 
            } else {
                menu_pointer = MAIN_MENU_NUM; 
            }
            break;
    }

    old_menu_pointer = 255; // Примусове перемальовування дисплея
}

// ==================== ЛОГІКА КНОПОК У МЕНЮ ====================
void buttons_in_menu_process(uint32_t butt_num, bool is_long_press) {
    if (menu_pointer == MYSPACE_MENU_NUM) {
        process_myspace_buttons(butt_num);
        return;
    }

    switch (butt_num) {
        case PIN_BTN_1: // Вгору / Збільшення значення
            switch (menu_pointer) {
                case MAIN_MENU_NUM:    if (main_menu_pointer > 0) main_menu_pointer--; break;
                case FILTERS_MENU_NUM: if (filters_menu_pointer > 0) filters_menu_pointer--; break;
                case EQ_MENU_NUM:      if (eq_menu_pointer > 0) eq_menu_pointer--; break;
                case PHONO_MENU_NUM:   if (phono_menu_pointer > 0) phono_menu_pointer--; break;
            }
            old_menu_pointer = 255;
            break;

        case PIN_BTN_2: // Вниз / Зменшення значення
            switch (menu_pointer) {
                case MAIN_MENU_NUM:    if (main_menu_pointer < 5) main_menu_pointer++; break;
                case FILTERS_MENU_NUM: if (filters_menu_pointer < 5) filters_menu_pointer++; break;
                case EQ_MENU_NUM:      if (eq_menu_pointer < 5) eq_menu_pointer++; break;
                case PHONO_MENU_NUM:   if (phono_menu_pointer < 1) phono_menu_pointer++; break;
            }
            old_menu_pointer = 255;
            break;

        case PIN_BTN_3: // Вибір / Підтвердження
            switch (menu_pointer) {
                case MAIN_MENU_NUM:
                    switch (main_menu_pointer) {
                        case 0: menu_pointer = EQ_MENU_NUM; break;
                        case 1: menu_pointer = FILTERS_MENU_NUM; break;
                        case 2: 
                            menu_pointer = MYSPACE_MENU_NUM; 
                            myspace_depth = 0;
                            myspace_speaker_pointer = 0;
                            myspace_param_pointer = 0;
                            break;
                        case 3: 
                            saved_spatial = !saved_spatial;
                            // ESP_LOGI(TAG_BTN, "[SETTING ACTIVATED] Spatial set to: %s", spatial_names[saved_spatial]);
                            old_menu_pointer = 255;
                            break;
                        case 4: 
                            saved_sys = !saved_sys;
                            // ESP_LOGI(TAG_BTN, "[SETTING ACTIVATED] System Mode set to: %s", sys_names[saved_sys]);
                            old_menu_pointer = 255;
                            break;
                        case 5: menu_pointer = PHONO_MENU_NUM; break;
                    }
                    break;

                case EQ_MENU_NUM:
                    saved_eq_preset = eq_menu_pointer;
                    // ESP_LOGI(TAG_BTN, "[SETTING ACTIVATED] EQ Preset: %d (%s)", saved_eq_preset, eq_names[saved_eq_preset]);
                    old_menu_pointer = 255;
                    break;

                case FILTERS_MENU_NUM:
                    saved_filter = filters_menu_pointer;
                    // ESP_LOGI(TAG_BTN, "[SETTING ACTIVATED] Filter: %d (%s)", saved_filter, filter_names[saved_filter]);
                    old_menu_pointer = 255;
                    break;

                case PHONO_MENU_NUM:
                    if (is_long_press) {
                        if (phono_menu_pointer == 0) {
                            // ESP_LOGI(TAG_BTN, "[ACTION TRIGGERED] ERASE NEEDLE TIME Executed!");
                        } else if (phono_menu_pointer == 1) {
                            // ESP_LOGI(TAG_BTN, "[ACTION TRIGGERED] ERASE TOTAL TIME Executed!");
                        }
                    }
                    break;
            }
            break;

        case PIN_BTN_4: // Назад / Вихід
            if (menu_pointer == MAIN_MENU_NUM) {
                exit_setup_menu();
            } else {
                menu_pointer = MAIN_MENU_NUM;
                old_menu_pointer = 255;
            }
            break;
    }
}

// ==================== РОЗПОДІЛ ПОДІЙ ====================
void handle_button_event(uint32_t btn_pin, bool is_long_press) {
    mark_button_activity();

    if (is_muted) {
        set_mute_state(false);
        return; 
    }

    if (current_state == STATE_IDLE_CAT2 || current_state == STATE_SPECTRUM) {
        if (btn_pin == PIN_BTN_1) {
            change_volume(1);
        } else if (btn_pin == PIN_BTN_2) {
            change_volume(-1);
        } else if (btn_pin == PIN_BTN_3 && is_long_press) {
            current_state = STATE_SETUP_MENU;
            menu_pointer = MAIN_MENU_NUM;
            old_menu_pointer = 255;
            load_menu_pointers_from_ram();
            last_menu_activity_tick = xTaskGetTickCount();
        }
    } 
    else if (current_state == STATE_VOLUME_POPUP) {
        if (btn_pin == PIN_BTN_1) {
            change_volume(1);
        } else if (btn_pin == PIN_BTN_2) {
            change_volume(-1);
        } else if (btn_pin == PIN_BTN_4) {
            current_state = (is_input_sig_flag == 1) ? STATE_SPECTRUM : STATE_IDLE_CAT2;
        }
    } 
    else if (current_state == STATE_SETUP_MENU) {
        last_menu_activity_tick = xTaskGetTickCount();
        buttons_in_menu_process(btn_pin, is_long_press);
    }
}

// ==================== ISR ТА ГОЛОВНА ТАСКА ====================
static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint32_t pin_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_event_queue, &pin_num, NULL);
}

static void button_task(void* arg) {
    uint32_t io_num;
    TickType_t last_press_time = 0;

    while (1) {
        if (xQueueReceive(gpio_event_queue, &io_num, pdMS_TO_TICKS(200))) {
            TickType_t now = xTaskGetTickCount();

            if (now - last_press_time > pdMS_TO_TICKS(150)) {
                last_press_time = now;

                if (io_num == PIN_BTN_4) {
                    TickType_t start_hold = xTaskGetTickCount();
                    bool is_power_off_hold = false;
                    bool is_mute_triggered = false;

                    while (gpio_get_level(PIN_BTN_4) == 1) {
                        TickType_t elapsed = xTaskGetTickCount() - start_hold;

                        if (!is_mute_triggered && elapsed >= pdMS_TO_TICKS(2000) && elapsed < pdMS_TO_TICKS(5000)) {
                            is_mute_triggered = true;
                            toggle_mute();
                        }
                        if (elapsed >= pdMS_TO_TICKS(5000)) {
                            is_power_off_hold = true;
                            break;
                        }
                        vTaskDelay(pdMS_TO_TICKS(40));
                    }

                    if (is_power_off_hold) {
                        // ESP_LOGW(TAG_BTN, "PIN_BTN_4 утримано 5 сек. Запуск анімації вимкнення...");
                        current_state = STATE_SLEEP_SHUTDOWN;
                    } else if (!is_mute_triggered) {
                        handle_button_event(PIN_BTN_4, false);
                    }
                }

                else if (io_num == PIN_BTN_3) {
                    bool is_long_press = false;
                    TickType_t start_hold = xTaskGetTickCount();

                    while (gpio_get_level(PIN_BTN_3) == 1) {
                        TickType_t elapsed = xTaskGetTickCount() - start_hold;
                        
                        // Якщо пройшло 2 секунди і ми ще не відправили подію
                        if (elapsed >= pdMS_TO_TICKS(2000) && !is_long_press) {
                            is_long_press = true;
                            handle_button_event(io_num, true); // Заходимо в меню ОДРАЗУ
                        }
                        vTaskDelay(pdMS_TO_TICKS(40));
                    }
                    
                    // Якщо кнопку відпустили раніше (менше ніж 2 сек) — це коротке натискання
                    if (!is_long_press) {
                        handle_button_event(io_num, false);
                    }
                    
                    // Оновлюємо час, щоб уникнути спрацьовування від брязкоту контактів при відпусканні
                    last_press_time = xTaskGetTickCount();
                }

                // Вмикаємо автоповтор для кнопок 1 та 2 не лише поза меню, а й під час зміни значення у MySpace (depth == 2)
                else if ((io_num == PIN_BTN_1 || io_num == PIN_BTN_2) && 
                        (current_state != STATE_SETUP_MENU || (menu_pointer == MYSPACE_MENU_NUM && myspace_depth == 2))) {
                    handle_button_event(io_num, false);
                    TickType_t start_hold = xTaskGetTickCount();
                    bool auto_repeat = false;

                    while (gpio_get_level(io_num) == 1) {
                        TickType_t elapsed = xTaskGetTickCount() - start_hold;
                        if (elapsed >= pdMS_TO_TICKS(1000)) {
                            auto_repeat = true;
                            break;
                        }
                        vTaskDelay(pdMS_TO_TICKS(40));
                    }

                    while (auto_repeat && gpio_get_level(io_num) == 1) {
                        handle_button_event(io_num, false);
                        vTaskDelay(pdMS_TO_TICKS(80)); 
                    }
                }

                else {
                    handle_button_event(io_num, false);
                }
            }
        }
        
        if (current_state == STATE_SETUP_MENU) {
            if ((xTaskGetTickCount() - last_menu_activity_tick) > pdMS_TO_TICKS(MENU_AUTO_EXIT_TIMEOUT_MS)) {
                exit_setup_menu();
            }
        }
    }
}

// ==================== ІНІЦІАЛІЗАЦІЯ ====================
void buttons_init(void) {
    button_activity_timer = xTimerCreate("BtnIdleTimer", 
                                         pdMS_TO_TICKS(10000), 
                                         pdFALSE, 
                                         NULL, 
                                         button_activity_timer_cb);
    leds_init();

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_BTN_1) | (1ULL << PIN_BTN_2) | (1ULL << PIN_BTN_3) | (1ULL << PIN_BTN_4),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE
    };
    gpio_config(&io_conf);

    gpio_event_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(button_task, "button_task", 4096, NULL, 10, NULL); 

    esp_err_t isr_err = gpio_install_isr_service(0);
    if (isr_err != ESP_OK && isr_err != ESP_ERR_INVALID_STATE) {
        // ESP_LOGE(TAG_BTN, "Failed to install GPIO ISR service");
    }

    gpio_isr_handler_add(PIN_BTN_1, gpio_isr_handler, (void*) PIN_BTN_1);
    gpio_isr_handler_add(PIN_BTN_2, gpio_isr_handler, (void*) PIN_BTN_2);
    gpio_isr_handler_add(PIN_BTN_3, gpio_isr_handler, (void*) PIN_BTN_3);
    gpio_isr_handler_add(PIN_BTN_4, gpio_isr_handler, (void*) PIN_BTN_4);
}