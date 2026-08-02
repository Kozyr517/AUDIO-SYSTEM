#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"

#include "gp1247ai.h"
#include "Font16x16.h"

static const char *TAG = "CATZILLA_DIAG";

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

void app_main(void) {
    ESP_LOGI(TAG, "=== СТАРОТ ДІАГНОСТИЧНОГО ТЕСТУ VFD ===");

    // 1. ПОДАЧА ЖИВЛЕННЯ
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
    gpio_set_level(PIN_BLE_EN, 1);
    gpio_set_level(PIN_EN_POW_ADAU, 1);
    gpio_set_level(PIN_EN_ADDR_LED, 1);

    ESP_LOGI(TAG, "Пауза 2 секунди для стабілізації 67V / 50V...");
    vTaskDelay(pdMS_TO_TICKS(2000));

    // 2. ІНІЦІАЛІЗАЦІЯ SPI (Використовуємо SPI_DMA_DISABLED для надійності)
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_SCLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 2100
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_DISABLED));

    spi_device_interface_config_t dev_config = {
        .clock_source = SOC_MOD_CLK_XTAL,
        .clock_speed_hz = 3000000,
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

    // 3. ЦИКЛ ДІАГНОСТИКИ СТАНІВ RS (HIGH / LOW)
    while (1) {
        // --- ТЕСТ 1: RS = HIGH (1) ---
        ESP_LOGI(TAG, "Тест RS = 1 (HIGH)");
        gpio_set_level(PIN_NUM_LCD_RS, 1);
        vTaskDelay(pdMS_TO_TICKS(50));

        GP1247AI_Ini(&lcd);
        LCD_clear(&lcd);
        LCD_print(&lcd, "RS = HIGH (1)", 5, 10, (const uint8_t*)Font16x16, 0);
        LCD_Update(&lcd);

        vTaskDelay(pdMS_TO_TICKS(3000)); // Пауза 3 сек

        // --- ТЕСТ 2: RS = LOW (0) ---
        ESP_LOGI(TAG, "Тест RS = 0 (LOW)");
        gpio_set_level(PIN_NUM_LCD_RS, 0);
        vTaskDelay(pdMS_TO_TICKS(50));

        GP1247AI_Ini(&lcd);
        LCD_clear(&lcd);
        LCD_print(&lcd, "RS = LOW (0)", 5, 10, (const uint8_t*)Font16x16, 0);
        LCD_Update(&lcd);

        vTaskDelay(pdMS_TO_TICKS(3000)); // Пауза 3 сек
    }
}