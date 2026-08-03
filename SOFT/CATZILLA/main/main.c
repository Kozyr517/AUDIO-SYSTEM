#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"

// Бібліотеки дисплея
#include "gp1247ai.h"
#include "Font16x16.h"

// Бібліотека з анімацією кота
#include "cat_animation.h"

static const char *TAG = "CATZILLA_BOOT";

// ==================== ПІНИ ЖИВЛЕННЯ ТА КЕРУВАННЯ ====================
#define PIN_VSYS_EN         GPIO_NUM_4
#define PIN_BLE_EN          GPIO_NUM_5
#define PIN_EN_POW_ADAU     GPIO_NUM_10
#define PIN_EN_ALL_POWER    GPIO_NUM_21
#define PIN_EN_ADDR_LED     GPIO_NUM_47

// ==================== ПІНИ VFD ДИСПЛЕЯ ====================
#define PIN_NUM_MOSI        GPIO_NUM_40
#define PIN_NUM_SCLK        GPIO_NUM_39
#define PIN_NUM_LCD_CS      GPIO_NUM_48
#define PIN_NUM_LCD_RS      GPIO_NUM_38

spi_device_handle_t g_spi_handle;
TypeDef_GP1247AI lcd;

// Передача даних через SPI
void dma_spi_transmit(uint8_t *data, size_t size) {
    spi_transaction_t trans = {
        .tx_buffer = (void *)data,
        .length = (8 * size)
    };
    spi_device_transmit(g_spi_handle, &trans);
}

void delay_ms_wrapper(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

// ==================== ФУНКЦІЯ-АДАПТЕР ДЛЯ КАРТИНОК ====================
// Правильне транспонування та упаковка байтів під формат сторінок GP1247AI
void draw_cat_frame(TypeDef_GP1247AI *p_lcd, uint16_t start_x, uint16_t start_y, const uint8_t *h_bitmap, uint16_t w, uint16_t h) {
    uint8_t v_bitmap[384]; // Буфер під кадр 64x48 (64 * (48/8) = 384 байти)
    memset(v_bitmap, 0, sizeof(v_bitmap));

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            // Отримуємо стан пікселя з монохромного бітмапа (горизонтальний формат)
            int h_byte = (y * w + x) / 8;
            int h_bit = 7 - (x % 8);
            uint8_t pixel = (h_bitmap[h_byte] >> h_bit) & 0x01;

            if (pixel) {
                // Розраховуємо сторінку та вертикальний біт для буфера GP1247AI
                int page = y / 8;
                int v_bit = 7 - (y % 8); // Інверсія біта для виправлення дзеркалення
                int v_byte = page * w + x;
                
                if (v_byte < sizeof(v_bitmap)) {
                    v_bitmap[v_byte] |= (1 << v_bit);
                }
            }
        }
    }
    // Відправляємо конвертований кадр
    LCD_DrawBitmap(p_lcd, start_x, start_y, v_bitmap, w, h, 1);
}

// ==================== АНІМАЦІЯ ЗАВАНТАЖЕННЯ ====================
void draw_boot_animation(TypeDef_GP1247AI *p_lcd) {
    uint16_t cat_x = (253 - CAT_ANIM_W) / 2; // ~94
    uint16_t cat_y = (64 - CAT_ANIM_H) / 2;  // ~8

    LCD_clear(p_lcd);
    LCD_Update(p_lcd);

    ESP_LOGI(TAG, "Запуск анімації кота...");

    // Крутимо анімацію 5 разів
    for (int loop = 0; loop < 5; loop++) {
        for (int frame = 0; frame < CAT_BOOT_FRAMES; frame++) {
            LCD_clear(p_lcd);
            
            draw_cat_frame(p_lcd, cat_x, cat_y, boot_cat_anim[frame], CAT_ANIM_W, CAT_ANIM_H);
            
            LCD_Update(p_lcd);
            vTaskDelay(pdMS_TO_TICKS(150));
        }
    }

    // Фінальний екран завантаження
    ESP_LOGI(TAG, "Анімацію завершено.");
    LCD_clear(p_lcd);
    LCD_print(p_lcd, "CATZILLA", (253 - (8 * 16)) / 2, 10, (const uint8_t*)Font16x16, 0);
    LCD_print(p_lcd, "ONLINE", (253 - (6 * 16)) / 2, 35, (const uint8_t*)Font16x16, 0);
    LCD_Update(p_lcd);
    
    vTaskDelay(pdMS_TO_TICKS(2000));
}

// ==================== MAIN ====================
void app_main(void) {
    ESP_LOGI(TAG, "=== СТАРТ СИСТЕМИ CATZILLA Pip-Boy ===");

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_EN_ALL_POWER) | (1ULL << PIN_VSYS_EN) |
                        (1ULL << PIN_BLE_EN) | (1ULL << PIN_EN_POW_ADAU) |
                        (1ULL << PIN_EN_ADDR_LED) | (1ULL << PIN_NUM_LCD_RS),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    gpio_set_level(PIN_EN_ALL_POWER, 1);
    gpio_set_level(PIN_VSYS_EN, 1);
    gpio_set_level(PIN_BLE_EN, 0);
    gpio_set_level(PIN_EN_POW_ADAU, 1);
    gpio_set_level(PIN_EN_ADDR_LED, 1);

    vTaskDelay(pdMS_TO_TICKS(2000));

    // ІНІЦІАЛІЗАЦІЯ SPI (DMA ВКЛЮЧЕНО)
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
        .clock_speed_hz = 2000000,
        .mode = 0,
        .spics_io_num = PIN_NUM_LCD_CS,
        .queue_size = 300,
        .post_cb = NULL,
        .pre_cb = NULL,
        .flags = SPI_DEVICE_TXBIT_LSBFIRST,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev_config, &g_spi_handle));

    lcd.dma_spi_transmit = dma_spi_transmit;
    lcd.delay_ms = delay_ms_wrapper;

    // ІНІЦІАЛІЗАЦІЯ ДИСПЛЕЯ
    gpio_set_level(PIN_NUM_LCD_RS, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    GP1247AI_Ini(&lcd);

    // ЗАПУСК АНІМАЦІЇ
    draw_boot_animation(&lcd);

    ESP_LOGI(TAG, "Готово! Перехід у фоновий режим.");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}