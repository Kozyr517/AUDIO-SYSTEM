#include "menu.h"
#include "Sinclair_S8x8.h"
#include "driver/spi_master.h"
#include "Font16x16.h"
#include "driver/gpio.h"
#include <string.h>
#include "rom/ets_sys.h"
#include "led_strip.h"
#include "esp_log.h"

#include "animation.h"
#include "anim_cat1.h"
#include "anim_cat2.h"
#include "anim_cat3.h"
#include "anim_cat4.h"
#include "anim_cat5.h"
#include "anim_cat6.h"
#include "anim_cat7.h"


// ================= ПІНИ КНОПОК ТА СВІТЛОДІОДІВ =================
#define PIN_BTN_1           GPIO_NUM_1   // Кнопка 1 -> Світлодіод 4 (індекс 3)
#define PIN_BTN_2           GPIO_NUM_2   // Кнопка 2 -> Світлодіод 3 (індекс 2)
#define PIN_BTN_3           GPIO_NUM_41  // Кнопка 3 -> Світлодіод 2 (індекс 1)
#define PIN_BTN_4           GPIO_NUM_42  // Кнопка 4 -> Світлодіод 1 (індекс 0)

#define PIN_ADDR_L_FRONT    GPIO_NUM_12
#define LED_COUNT_FRONT     4

// Піни живлення
#define PIN_VSYS_EN         GPIO_NUM_4
#define PIN_BLE_EN          GPIO_NUM_5
#define PIN_EN_POW_ADAU     GPIO_NUM_10
#define PIN_EN_ALL_POWER    GPIO_NUM_21
#define PIN_EN_ADDR_LED     GPIO_NUM_47

#define MAIN_MENU_NUM    0
#define FILTERS_MENU_NUM 1
#define EQ_MENU_NUM      2
#define BALANCE_MENU_NUM 3
#define PHONO_MENU_NUM   6

static const char *TAG = "MENU";

// Глобальні змінні
TypeDef_GP1247AI lcd_instance;
TypeDef_GP1247AI* lcd = &lcd_instance;
spi_device_handle_t g_spi_handle;

// Передача даних через SPI
void dma_spi_transmit(uint8_t *data, size_t size) {
    spi_transaction_t trans = {
        .tx_buffer = (void *)data,
        .length = (8 * size)
    };
    spi_device_transmit(g_spi_handle, &trans);
}

// ==================== ФУНКЦІЯ-АДАПТЕР ДЛЯ КАРТИНОК ====================
void draw_cat_frame(uint16_t start_x, uint16_t start_y, const uint8_t *h_bitmap, uint16_t w, uint16_t h) {
    uint8_t v_bitmap[384];
    memset(v_bitmap, 0, sizeof(v_bitmap));

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int h_byte = (y * w + x) / 8;
            int h_bit = 7 - (x % 8);
            uint8_t pixel = (h_bitmap[h_byte] >> h_bit) & 0x01;

            if (pixel) {
                int page = y / 8;
                int v_bit = 7 - (y % 8);
                int v_byte = page * w + x;
                if (v_byte < sizeof(v_bitmap)) {
                    v_bitmap[v_byte] |= (1 << v_bit);
                }
            }
        }
    }
    LCD_DrawBitmap(lcd, start_x, start_y, v_bitmap, w, h, 1);
}

// =================== АНІМАЦІЯ ЗАВАНТАЖЕННЯ ===================
void draw_boot_animation(void) {
    const int16_t SCREEN_WIDTH = 256; 
    const int16_t SCREEN_HEIGHT = 64;
    
    const int16_t cat_y = (SCREEN_HEIGHT - TEST_ANIM_H) / 2; 
    const int16_t start_x = 0;
    const int16_t end_x = SCREEN_WIDTH - TEST_ANIM_W;
    const int16_t step_x = 4;                        
    const uint32_t delay_ms = 60;                    

    ESP_LOGI(TAG, "Запуск тестової анімації...");

    LCD_clear(lcd);
    LCD_Update(lcd);

    uint8_t current_frame = 0;

    for (int16_t x = start_x; x <= end_x; x += step_x) {
        LCD_clear(lcd); 
        draw_cat_frame(x, cat_y, test_anim[current_frame], TEST_ANIM_W, TEST_ANIM_H);
        LCD_Update(lcd); 

        vTaskDelay(pdMS_TO_TICKS(delay_ms));

        current_frame++;
        if (current_frame >= TEST_FRAMES) {
            current_frame = 0;
        }
    }

    LCD_print(lcd, "CATZILLA", (253 - (8 * 16)) / 2, 10, (const uint8_t*)Font16x16, 0);
    LCD_print(lcd, "ONLINE", (253 - (6 * 16)) / 2, 35, (const uint8_t*)Font16x16, 0);
    
    LCD_Update(lcd);
    vTaskDelay(pdMS_TO_TICKS(2000));
}

void delay_ms_wrapper(uint32_t ms) {
    esp_rom_delay_us(ms * 1000);
}

// static uint32_t press_start_tick = 0; // Закоментовано, оскільки змінна не використовувалася
static bool button_held = false;

// ================= КОЛЬОРИ ТА LED (10% ЯСКРАВОСТІ) =================
typedef struct { float r, g, b; } Color;

static const Color COLOR_IDLE   = {4.0f, 24.0f, 9.0f};  
static const Color COLOR_ACTIVE = {25.5f, 18.0f, 0.0f};

static Color current_colors[LED_COUNT_FRONT];
static Color target_colors[LED_COUNT_FRONT];
static led_strip_handle_t strip_front;

typedef struct {
    gpio_num_t gpio;
    int led_index;
} ButtonMapping;

static const ButtonMapping btn_map[LED_COUNT_FRONT] = {
    { PIN_BTN_1, 3 }, // GPIO 1  -> LED 4
    { PIN_BTN_2, 2 }, // GPIO 2  -> LED 3
    { PIN_BTN_3, 1 }, // GPIO 41 -> LED 2
    { PIN_BTN_4, 0 }  // GPIO 42 -> LED 1
};

uint8_t main_menu_pointer = 0;
uint8_t filters_menu_pointer = 0;
uint8_t eq_menu_pointer = 0;
uint8_t balance_menu_pointer = 0;
uint8_t phono_menu_pointer = 0;

uint8_t menu_pointer = 0;

uint8_t old_menu_pointer = 255;
uint8_t old_main_menu_pointer = 0;
uint8_t old_filters_menu_pointer = 0;
uint8_t old_eq_menu_pointer = 0;
uint8_t old_balance_menu_pointer = 0;
uint8_t old_phono_menu_pointer = 0;

const uint8_t eq_pop[10]    = {29, 40, 44, 45, 41, 30, 28, 28, 29, 29};
const uint8_t eq_rock[10]   = {45, 40, 23, 19, 26, 39, 47, 50, 50, 50};
const uint8_t eq_jazz[10]   = {40, 38, 32, 33, 31, 35, 39, 41, 43, 44};
const uint8_t eq_symph[10]  = {49, 49, 42, 42, 33, 24, 24, 24, 33, 33};
const uint8_t eq_nature[10] = {33, 33, 33, 33, 33, 33, 33, 33, 33, 33};
const uint8_t eq_bass[10]   = {48, 48, 48, 42, 35, 25, 18, 15, 14, 14};

const uint8_t* eq_presets[6] = {eq_pop, eq_rock, eq_jazz, eq_symph, eq_nature, eq_bass};
const char* eq_names[6] = {"POP", "ROCK", "JAZZ", "SYMPH", "NATURE", "BASS"};
const char* balance_names[3] = {"VOL", "L/R", "F/B"};
const char* phono_names[3]  = {"NEEDLE", "TOTAL", "ERASE"};

void display_init() {
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_SCLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 2100
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev_config = {
        .clock_source = SOC_MOD_CLK_XTAL,
        .clock_speed_hz = 2500000,
        .mode = 0,
        .spics_io_num = PIN_NUM_LCD_CS,
        .queue_size = 300,
        .post_cb = NULL,
        .pre_cb = NULL,
        .flags = SPI_DEVICE_TXBIT_LSBFIRST,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev_config, &g_spi_handle));

    lcd->dma_spi_transmit = dma_spi_transmit;
    lcd->delay_ms = delay_ms_wrapper;

    gpio_set_level(PIN_NUM_LCD_RS, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    GP1247AI_Ini(lcd);

    draw_boot_animation();
}

void set_main_menu() {
    LCD_clear(lcd);

    switch (main_menu_pointer) {
        case 0: LCD_DrawRect(lcd, 1, 1, 92, 20, 1); break;
        case 1: LCD_DrawRect(lcd, 1, 22, 92, 20, 1); break;
        case 2: LCD_DrawRect(lcd, 1, 43, 92, 20, 1); break;
        case 3: LCD_DrawRect(lcd, 95, 1, 92, 20, 1); break;
        case 4: LCD_DrawRect(lcd, 95, 22, 92, 20, 1); break;
        case 5: LCD_DrawRect(lcd, 95, 43, 92, 20, 1); break;
    }

    LCD_DrawRect(lcd, 2, 2, 90, 18, 1);
    LCD_print(lcd, "EQ PRESET", 11, 7, (const uint8_t*)Sinclair_S8x8, 0);

    LCD_DrawRect(lcd, 2, 23, 90, 18, 1);
    LCD_print(lcd, "FILTERS", 19, 28, (const uint8_t*)Sinclair_S8x8, 0);

    LCD_DrawRect(lcd, 2, 44, 90, 18, 1);
    LCD_print(lcd, "VOLUME/BAL", 7, 49, (const uint8_t*)Sinclair_S8x8, 0);

    LCD_DrawRect(lcd, 96, 2, 90, 18, 1);
    LCD_print(lcd, "SPATIAL 3D", 101, 7, (const uint8_t*)Sinclair_S8x8, 0);

    LCD_DrawRect(lcd, 96, 23, 90, 18, 1);
    LCD_print(lcd, "NIGHT MOD", 105, 28, (const uint8_t*)Sinclair_S8x8, 0);

    LCD_DrawRect(lcd, 96, 44, 90, 18, 1);
    LCD_print(lcd, "PHONO MM", 109, 49, (const uint8_t*)Sinclair_S8x8, 0);

    LCD_DrawRect(lcd, 190, 2, 60, 60, 1);
    LCD_Update(lcd);
}

void set_filters_menu() {
    LCD_clear(lcd);

    switch (filters_menu_pointer) {
        case 0: LCD_DrawRect(lcd, 1, 1, 68, 20, 1); break;
        case 1: LCD_DrawRect(lcd, 1, 22, 68, 20, 1); break;
        case 2: LCD_DrawRect(lcd, 1, 43, 68, 20, 1); break;
        case 3: LCD_DrawRect(lcd, 69, 1, 68, 20, 1); break;
        case 4: LCD_DrawRect(lcd, 69, 22, 68, 20, 1); break;
        case 5: LCD_DrawRect(lcd, 69, 43, 68, 20, 1); break;
    }

    LCD_DrawRect(lcd, 2, 2, 66, 18, 1);
    LCD_print(lcd, "SILK", 19, 7, (const uint8_t*)Sinclair_S8x8, 0);

    LCD_DrawRect(lcd, 2, 23, 66, 18, 1);
    LCD_print(lcd, "PURRITY", 7, 28, (const uint8_t*)Sinclair_S8x8, 0);

    LCD_DrawRect(lcd, 2, 44, 66, 18, 1);
    LCD_print(lcd, "DRIVE", 15, 49, (const uint8_t*)Sinclair_S8x8, 0);

    LCD_DrawRect(lcd, 70, 2, 66, 18, 1);
    LCD_print(lcd, "ATMOS", 83, 7, (const uint8_t*)Sinclair_S8x8, 0);

    LCD_DrawRect(lcd, 70, 23, 66, 18, 1);
    LCD_print(lcd, "VILVET", 79, 28, (const uint8_t*)Sinclair_S8x8, 0);

    LCD_DrawRect(lcd, 70, 44, 66, 18, 1);
    LCD_print(lcd, "DIRECT", 79, 49, (const uint8_t*)Sinclair_S8x8, 0);

    LCD_DrawRect(lcd, 139, 2, 111, 60, 1);
    LCD_Update(lcd);
}

void set_eq_menu() {
    LCD_clear(lcd);
    LCD_print(lcd, eq_names[eq_menu_pointer], 118, 4, (const uint8_t*)Sinclair_S8x8, 0);

    for (int px = 117; px < 251; px += 4) {
        LCD_DrawPixel(lcd, px, 29, 1);
    }

    int prev_x = 0;
    int prev_y = 0;

    for (int i = 0; i < 10; i++) {
        int x = 121 + i * 14;
        int y = 62 - eq_presets[eq_menu_pointer][i];

        LCD_DrawRect(lcd, x - 1, y - 1, 3, 3, 1);

        if (i > 0) {
            LCD_DrawLine(lcd, prev_x, prev_y, x, y, 1);
        }

        prev_x = x;
        prev_y = y;
    }

    switch (eq_menu_pointer) {
        case 0: LCD_DrawRect(lcd, 1, 1, 52, 20, 1); break;
        case 1: LCD_DrawRect(lcd, 1, 22, 52, 20, 1); break;
        case 2: LCD_DrawRect(lcd, 1, 43, 52, 20, 1); break;
        case 3: LCD_DrawRect(lcd, 55, 1, 58, 20, 1); break;
        case 4: LCD_DrawRect(lcd, 55, 22, 58, 20, 1); break;
        case 5: LCD_DrawRect(lcd, 55, 43, 58, 20, 1); break;
    }

    LCD_DrawRect(lcd, 2, 2, 50, 18, 1);
    LCD_print(lcd, "POP", 16, 7, (const uint8_t*)Sinclair_S8x8, 0);

    LCD_DrawRect(lcd, 2, 23, 50, 18, 1);
    LCD_print(lcd, "ROCK", 12, 28, (const uint8_t*)Sinclair_S8x8, 0);

    LCD_DrawRect(lcd, 2, 44, 50, 18, 1);
    LCD_print(lcd, "JAZZ", 11, 49, (const uint8_t*)Sinclair_S8x8, 0);

    LCD_DrawRect(lcd, 56, 2, 56, 18, 1);
    LCD_print(lcd, "SYMPH", 65, 7, (const uint8_t*)Sinclair_S8x8, 0);

    LCD_DrawRect(lcd, 56, 23, 56, 18, 1);
    LCD_print(lcd, "NATURE", 61, 28, (const uint8_t*)Sinclair_S8x8, 0);

    LCD_DrawRect(lcd, 56, 44, 56, 18, 1);
    LCD_print(lcd, "BASS", 69, 49, (const uint8_t*)Sinclair_S8x8, 0);

    LCD_DrawRect(lcd, 115, 2, 137, 60, 1);
    LCD_Update(lcd);
}

void set_balance_menu() {
    LCD_clear(lcd);
    LCD_print(lcd, balance_names[balance_menu_pointer], 118, 4, (const uint8_t*)Sinclair_S8x8, 0);

    switch (balance_menu_pointer) {
        case 0: LCD_DrawRect(lcd, 1, 1, 112, 20, 1); break;
        case 1: LCD_DrawRect(lcd, 1, 22, 112, 20, 1); break;
        case 2: LCD_DrawRect(lcd, 1, 43, 112, 20, 1); break;
    }

    LCD_DrawRect(lcd, 2, 2, 110, 18, 1);
    LCD_print(lcd, "MASTER VOLUME", 11, 7, (const uint8_t*)Sinclair_S8x8, 0);

    LCD_DrawRect(lcd, 2, 23, 110, 18, 1);
    LCD_print(lcd, "BALANCE LEFT/RIGHT", 7, 28, (const uint8_t*)Sinclair_S8x8, 0);

    LCD_DrawRect(lcd, 2, 44, 110, 18, 1);
    LCD_print(lcd, "BALANCE FRONT/BACK", 7, 49, (const uint8_t*)Sinclair_S8x8, 0);

    LCD_DrawRect(lcd, 115, 2, 137, 60, 1);
    LCD_Update(lcd);
}

void set_phono_menu() {
    LCD_clear(lcd);

    switch (phono_menu_pointer) {
        case 0: LCD_DrawRect(lcd, 1, 16, 152, 15, 1); break;
        case 1: LCD_DrawRect(lcd, 1, 46, 152, 15, 1); break;
    }

    LCD_DrawRect(lcd, 2, 2, 150, 13, 1);
    LCD_print(lcd, "000.015h30m", 42, 4, (const uint8_t*)Sinclair_S8x8, 0);

    LCD_DrawRect(lcd, 2, 17, 150, 13, 1);
    LCD_print(lcd, "ERASE NEEDLE TIME", 10, 19, (const uint8_t*)Sinclair_S8x8, 0);

    LCD_DrawRect(lcd, 2, 32, 150, 13, 1);
    LCD_print(lcd, "000.100h10m", 42, 34, (const uint8_t*)Sinclair_S8x8, 0);

    LCD_DrawRect(lcd, 2, 47, 150, 13, 1);
    LCD_print(lcd, "ERASE TOTAL TIME", 14, 49, (const uint8_t*)Sinclair_S8x8, 0);

    LCD_DrawRect(lcd, 155, 2, 95, 60, 1);
    LCD_Update(lcd);
}

void menu_update() {
    if (menu_pointer != old_menu_pointer || main_menu_pointer != old_main_menu_pointer ||
        filters_menu_pointer != old_filters_menu_pointer || eq_menu_pointer != old_eq_menu_pointer || 
        balance_menu_pointer != old_balance_menu_pointer || phono_menu_pointer != old_phono_menu_pointer) {

        old_eq_menu_pointer = eq_menu_pointer;
        old_filters_menu_pointer = filters_menu_pointer;
        old_main_menu_pointer = main_menu_pointer;
        old_menu_pointer = menu_pointer;
        old_balance_menu_pointer = balance_menu_pointer;
        old_phono_menu_pointer = phono_menu_pointer;

        switch (menu_pointer) {
            case MAIN_MENU_NUM:    set_main_menu(); break;
            case FILTERS_MENU_NUM: set_filters_menu(); break;
            case EQ_MENU_NUM:      set_eq_menu(); break;
            case BALANCE_MENU_NUM: set_balance_menu(); break;
            case PHONO_MENU_NUM:   set_phono_menu(); break;    
        }
    }
}

void buttons_in_menu_process(uint32_t butt_num) {
    switch (butt_num) {
        case PIN_BTN_1: // UP
            switch (menu_pointer) {
                case MAIN_MENU_NUM:    if (main_menu_pointer > 0) main_menu_pointer -= 1; break;
                case FILTERS_MENU_NUM: if (filters_menu_pointer > 0) filters_menu_pointer -= 1; break;
                case EQ_MENU_NUM:      if (eq_menu_pointer > 0) eq_menu_pointer -= 1; break;
                case BALANCE_MENU_NUM: if (balance_menu_pointer > 0) balance_menu_pointer -= 1; break;
                case PHONO_MENU_NUM:   if (phono_menu_pointer > 0) phono_menu_pointer -= 1; break;
            }
            break;

        case PIN_BTN_2: // DOWN
            switch (menu_pointer) {
                case MAIN_MENU_NUM:    if (main_menu_pointer < 5) main_menu_pointer += 1; break;
                case FILTERS_MENU_NUM: if (filters_menu_pointer < 5) filters_menu_pointer += 1; break;
                case EQ_MENU_NUM:      if (eq_menu_pointer < 5) eq_menu_pointer += 1; break;
                case BALANCE_MENU_NUM: if (balance_menu_pointer < 5) balance_menu_pointer += 1; break;
                case PHONO_MENU_NUM:   if (phono_menu_pointer < 1) phono_menu_pointer += 1; break;
            }
            break;

        case PIN_BTN_3: // OK
            if (menu_pointer == PHONO_MENU_NUM) {
                ESP_LOGI(TAG, "Кнопку затиснуто, утримуйте 3 секунди...");
                bool held_successfully = true;

                for (int i = 0; i < 60; i++) {
                    vTaskDelay(pdMS_TO_TICKS(50));
                    if (gpio_get_level(PIN_BTN_3) == 0) { 
                        held_successfully = false;
                        break;
                    }
                }

                if (held_successfully) {
                    if (phono_menu_pointer == 0) {
                        ESP_LOGI(TAG, "Скидання часу голки виконано успішно!");
                    } else if (phono_menu_pointer == 1) {
                        ESP_LOGI(TAG, "Скидання загального часу виконано успішно!");
                    }
                } else {
                    ESP_LOGI(TAG, "Кнопку відпущено занадто рано. Дія скасована.");
                }
                break;
            }

            switch (menu_pointer) {
                case MAIN_MENU_NUM:
                    switch (main_menu_pointer) {
                        case 0: menu_pointer = EQ_MENU_NUM; break;
                        case 1: menu_pointer = FILTERS_MENU_NUM; break;
                        case 2: menu_pointer = BALANCE_MENU_NUM; break;
                        case 3: ESP_LOGW(TAG, "FUTURE UPDATE: SPATIAL 3D"); break;
                        case 4: ESP_LOGW(TAG, "FUTURE UPDATE: NIGHT MOD"); break;
                        case 5: menu_pointer = PHONO_MENU_NUM; break;
                    }
                    break;
                case FILTERS_MENU_NUM:
                case EQ_MENU_NUM:
                case BALANCE_MENU_NUM:
                    ESP_LOGW(TAG, "FUTURE UPDATE");
                    break;
            }
            break;

        case PIN_BTN_4: // ESC
            button_held = false;
            switch (menu_pointer) {
                case MAIN_MENU_NUM:
                    ESP_LOGW(TAG, "FUTURE UPDATE");
                    break;
                case FILTERS_MENU_NUM:
                case EQ_MENU_NUM:
                case BALANCE_MENU_NUM:
                case PHONO_MENU_NUM:
                    menu_pointer = MAIN_MENU_NUM;
                    break;
            }
            break;
    }
}

static QueueHandle_t gpio_event_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint32_t pin_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_event_queue, &pin_num, NULL);
}

static void button_task(void* arg) {
    uint32_t io_num;
    TickType_t last_press_time = 0;

    while (1) {
        if (xQueueReceive(gpio_event_queue, &io_num, portMAX_DELAY)) {
            TickType_t now = xTaskGetTickCount();

            if (now - last_press_time > pdMS_TO_TICKS(50)) {
                last_press_time = now;
                ESP_LOGI(TAG, "Pressed button num: %ld", io_num);
                buttons_in_menu_process(io_num);
            }
        }
    }
}

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
        
        if (strip_front) {
            led_strip_refresh(strip_front);
        }
        
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

void buttons_init() {
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

void leds_init() {
    gpio_config_t pwr_conf = {
        .pin_bit_mask = (1ULL << PIN_VSYS_EN) | (1ULL << PIN_BLE_EN) |
                        (1ULL << PIN_EN_POW_ADAU) | (1ULL << PIN_EN_ALL_POWER) |
                        (1ULL << PIN_EN_ADDR_LED),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&pwr_conf);
    
    gpio_set_level(PIN_VSYS_EN, 1);
    gpio_set_level(PIN_BLE_EN, 1);
    gpio_set_level(PIN_EN_POW_ADAU, 1);
    gpio_set_level(PIN_EN_ALL_POWER, 1);
    gpio_set_level(PIN_EN_ADDR_LED, 1);
    
    vTaskDelay(pdMS_TO_TICKS(20));

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
    ESP_LOGI(TAG, "Світлодіоди ініціалізовано!");
}