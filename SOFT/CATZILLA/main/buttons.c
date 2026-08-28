#include "buttons.h"
#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "led_strip.h"
#include "menu.h"
#include "app_state.h"

extern uint8_t is_input_sig_flag;

// ==================== ДОДАНІ ЗМІННІ ТА НАЛАШТУВАННЯ АВТОВИХОДУ ====================
#define MENU_AUTO_EXIT_TIMEOUT_MS 10000 // Час автовиходу з будь-якого меню/підменю (10 сек)
static TickType_t last_menu_activity_tick = 0;

// Функція для повного скидання меню у початковий стан
static void reset_menu_pointers(void) {
    menu_pointer = MAIN_MENU_NUM;
    main_menu_pointer = 0;
    filters_menu_pointer = 0;
    eq_menu_pointer = 0;
    balance_menu_pointer = 0;
    phono_menu_pointer = 0;
    old_menu_pointer = 255; // Форсуємо повну перемальовку при наступному вході
}

// Функція виходу з меню назад на робочий екран (Спектр або Очікування)
static void exit_setup_menu(void) {
    current_state = (is_input_sig_flag == 1) ? STATE_SPECTRUM : STATE_IDLE_CAT2;
    reset_menu_pointers();
}

// ==================== ДАНІ ТА СТРУКТУРИ LED ====================
typedef struct { float r, g, b; } Color;
static const Color COLOR_IDLE   = {4.0f, 24.0f, 9.0f};  
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

// ==================== ЛОГІКА ПІДСВІТКИ ====================
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

// ==================== ЛОГІКА КНОПОК У МЕНЮ ====================
void buttons_in_menu_process(uint32_t butt_num) {
    switch (butt_num) {
        case PIN_BTN_1: 
            switch (menu_pointer) {
                case MAIN_MENU_NUM:    if (main_menu_pointer > 0) main_menu_pointer--; break;
                case FILTERS_MENU_NUM: if (filters_menu_pointer > 0) filters_menu_pointer--; break;
                case EQ_MENU_NUM:      if (eq_menu_pointer > 0) eq_menu_pointer--; break;
                case BALANCE_MENU_NUM: if (balance_menu_pointer > 0) balance_menu_pointer--; break;
                case PHONO_MENU_NUM:   if (phono_menu_pointer > 0) phono_menu_pointer--; break;
            }
            break;

        case PIN_BTN_2: 
            switch (menu_pointer) {
                case MAIN_MENU_NUM:    if (main_menu_pointer < 5) main_menu_pointer++; break;
                case FILTERS_MENU_NUM: if (filters_menu_pointer < 5) filters_menu_pointer++; break;
                case EQ_MENU_NUM:      if (eq_menu_pointer < 5) eq_menu_pointer++; break;
                case BALANCE_MENU_NUM: if (balance_menu_pointer < 5) balance_menu_pointer++; break;
                case PHONO_MENU_NUM:   if (phono_menu_pointer < 1) phono_menu_pointer++; break;
            }
            break;

        case PIN_BTN_3: 
            switch (menu_pointer) {
                case MAIN_MENU_NUM:
                    switch (main_menu_pointer) {
                        case 0: menu_pointer = EQ_MENU_NUM; break;
                        case 1: menu_pointer = FILTERS_MENU_NUM; break;
                        case 2: menu_pointer = BALANCE_MENU_NUM; break;
                        case 5: menu_pointer = PHONO_MENU_NUM; break;
                    }
                    break;
            }
            break;

        case PIN_BTN_4: 
            if (menu_pointer == MAIN_MENU_NUM) {
                exit_setup_menu(); // Вихід з головного меню в режим спектра / очікування
            } else {
                menu_pointer = MAIN_MENU_NUM; // Повернення з підменю в головне меню
            }
            break;
    }
}

void handle_button_event(uint32_t btn_pin, bool is_long_press) {
    if (current_state == STATE_IDLE_CAT2 || current_state == STATE_SPECTRUM) {
        if (btn_pin == PIN_BTN_1) {
            if (master_volume < 100) master_volume++;
            current_state = STATE_VOLUME_POPUP;
            last_vol_activity_tick = xTaskGetTickCount();
        } 
        else if (btn_pin == PIN_BTN_2) {
            if (master_volume > 0) master_volume--;
            current_state = STATE_VOLUME_POPUP;
            last_vol_activity_tick = xTaskGetTickCount();
        } 
        else if (btn_pin == PIN_BTN_3 && is_long_press) {
            current_state = STATE_SETUP_MENU;
            reset_menu_pointers();
            last_menu_activity_tick = xTaskGetTickCount(); // Фіксуємо час входу в меню
        }
    } 
    else if (current_state == STATE_VOLUME_POPUP) {
        if (btn_pin == PIN_BTN_1) {
            if (master_volume < 100) master_volume++;
            last_vol_activity_tick = xTaskGetTickCount();
        } 
        else if (btn_pin == PIN_BTN_2) {
            if (master_volume > 0) master_volume--;
            last_vol_activity_tick = xTaskGetTickCount();
        } 
        else if (btn_pin == PIN_BTN_4) {
            current_state = (is_input_sig_flag == 1) ? STATE_SPECTRUM : STATE_IDLE_CAT2;
        }
    } 
    else if (current_state == STATE_SETUP_MENU) {
        // Оновлюємо таймер активності при натисканні БУДЬ-ЯКОЇ кнопки у БУДЬ-ЯКОМУ підменю
        last_menu_activity_tick = xTaskGetTickCount(); 
        buttons_in_menu_process(btn_pin);
    }
}

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint32_t pin_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_event_queue, &pin_num, NULL);
}

static void button_task(void* arg) {
    uint32_t io_num;
    TickType_t last_press_time = 0;

    while (1) {
        // Чекаємо подію кнопок не більше 200 мс, щоб регулярно перевіряти автовихід
        if (xQueueReceive(gpio_event_queue, &io_num, pdMS_TO_TICKS(200))) {
            TickType_t now = xTaskGetTickCount();

            if (now - last_press_time > pdMS_TO_TICKS(150)) {
                last_press_time = now;
                bool is_long_press = false;

                if (io_num == PIN_BTN_3) {
                    TickType_t start_hold = xTaskGetTickCount();
                    while (gpio_get_level(PIN_BTN_3) == 1) {
                        if ((xTaskGetTickCount() - start_hold) >= pdMS_TO_TICKS(2000)) {
                            is_long_press = true;
                            break;
                        }
                        vTaskDelay(pdMS_TO_TICKS(40));
                    }
                }

                handle_button_event(io_num, is_long_press);
            }
        }
        
        // ==================== АВТОВИХІД З БУДЬ-ЯКОГО МЕНЮ / ПІДМЕНЮ ====================
        if (current_state == STATE_SETUP_MENU) {
            if ((xTaskGetTickCount() - last_menu_activity_tick) > pdMS_TO_TICKS(MENU_AUTO_EXIT_TIMEOUT_MS)) {
                exit_setup_menu(); // Автоматично повертаємося на головний екран
            }
        }
    }
}

void buttons_init(void) {
    leds_init();

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_BTN_1) | (1ULL << PIN_BTN_2) | (1ULL << PIN_BTN_3) | (1ULL << PIN_BTN_4),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE
    };
    gpio_config(&io_conf);

    gpio_event_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(button_task, "button_task", 3072, NULL, 10, NULL);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_BTN_1, gpio_isr_handler, (void*) PIN_BTN_1);
    gpio_isr_handler_add(PIN_BTN_2, gpio_isr_handler, (void*) PIN_BTN_2);
    gpio_isr_handler_add(PIN_BTN_3, gpio_isr_handler, (void*) PIN_BTN_3);
    gpio_isr_handler_add(PIN_BTN_4, gpio_isr_handler, (void*) PIN_BTN_4);
}