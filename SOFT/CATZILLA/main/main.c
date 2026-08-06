#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "menu.h"

// ==================== ПІНИ ЖИВЛЕННЯ ТА КЕРУВАННЯ ====================
#define PIN_VSYS_EN         GPIO_NUM_4
#define PIN_BLE_EN          GPIO_NUM_5
#define PIN_EN_POW_ADAU     GPIO_NUM_10
#define PIN_EN_ALL_POWER    GPIO_NUM_21
#define PIN_EN_ADDR_LED     GPIO_NUM_47

static const char *TAG = "MAIN";

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

    display_init();
    buttons_init();

    ESP_LOGI(TAG, "Готово! Перехід у фоновий режим.");

    while (1) {
        menu_update();
        vTaskDelay(pdMS_TO_TICKS(50));  // Зміна пресету кожні 5 секунд
    }
}
