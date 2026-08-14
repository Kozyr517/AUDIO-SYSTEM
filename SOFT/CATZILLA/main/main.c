#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "menu.h"
#include "ds18b20.h"

// ==================== ПІНИ ЖИВЛЕННЯ ТА КЕРУВАННЯ ====================
#define PIN_VSYS_EN         GPIO_NUM_4
#define PIN_BLE_EN          GPIO_NUM_5
#define PIN_EN_POW_ADAU     GPIO_NUM_10
#define PIN_EN_ALL_POWER    GPIO_NUM_21
#define PIN_EN_ADDR_LED     GPIO_NUM_47

// Піни вентиляторів
#define PIN_FAN_1           GPIO_NUM_13
#define PIN_FAN_2           GPIO_NUM_14

// Пін підключення дата-лінії DS18B20
#define PIN_DS18B20         GPIO_NUM_15

static const char *TAG = "MAIN";

// ==================== СТРУКТУРА ТА АДРЕСИ ДАТЧИКІВ ====================
typedef struct {
    const char *name;
    uint8_t addr[8];
    float temp;
} sensor_t;

// Прив'язка унікальних hardware-адрес до фізичних зон
sensor_t g_sensors[] = {
    { .name = "Силова/цифрова камера",  .addr = {0x28, 0x08, 0x8E, 0x9C, 0x00, 0x00, 0x00, 0x25}, .temp = 0.0f },
    { .name = "Радіатор підсилювача 1", .addr = {0x28, 0xA3, 0xCB, 0x9C, 0x00, 0x00, 0x00, 0xBA}, .temp = 0.0f },
    { .name = "Радіатор підсилювача 2", .addr = {0x28, 0x90, 0x78, 0x9C, 0x00, 0x00, 0x00, 0x0F}, .temp = 0.0f },
    { .name = "Камера ADAU",            .addr = {0x28, 0xE6, 0xC4, 0x9C, 0x00, 0x00, 0x00, 0x4B}, .temp = 0.0f },
};

#define SENSOR_COUNT (sizeof(g_sensors) / sizeof(g_sensors[0]))

// ==================== ФОНОВА ЗАДАЧА ТЕМПЕРАТУРИ ====================
static void temperature_task(void *pvParameters) {
    // Ініціалізуємо 1-Wire пін
    ds18b20_init(PIN_DS18B20);

    // Фоновий цикл опитування
    while (1) {
        // 1. Одночасна команда на вимірювання для всіх датчиків на шині (Skip ROM + Convert T)
        ds18b20_request_temperature(PIN_DS18B20); 
        
        // 2. Фонове очікування 800 мс (поки йде вимірювання)
        vTaskDelay(pdMS_TO_TICKS(800)); 

        // 3. Почергове зчитування готових даних за жорстко прописаними адресами
        for (int i = 0; i < SENSOR_COUNT; i++) {
            if (ds18b20_read_temperature_addr(PIN_DS18B20, g_sensors[i].addr, &g_sensors[i].temp) == ESP_OK) {
                // Виводимо форматований рядок із назвою зони
                ESP_LOGI(TAG, "%-25s | Темп: %5.2f °C", g_sensors[i].name, g_sensors[i].temp);
            } else {
                // Якщо датчик відвалився, логуємо помилку для конкретної зони
                ESP_LOGE(TAG, "Помилка зв'язку з датчиком: %s", g_sensors[i].name);
            }
        }
        
        // Пауза між циклами вимірювання
        vTaskDelay(pdMS_TO_TICKS(2000)); 
    }
}

// ==================== MAIN ====================
void app_main(void) {
    ESP_LOGI(TAG, "=== СТАРТ СИСТЕМИ CATZILLA ===");

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_EN_ALL_POWER) | (1ULL << PIN_VSYS_EN) |
                        (1ULL << PIN_BLE_EN) | (1ULL << PIN_EN_POW_ADAU) |
                        (1ULL << PIN_EN_ADDR_LED) | (1ULL << PIN_NUM_LCD_RS) |
                        (1ULL << PIN_FAN_1) | (1ULL << PIN_FAN_2), // <--- Додали сюди
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    gpio_set_level(PIN_EN_ALL_POWER, 1);
    gpio_set_level(PIN_VSYS_EN, 1);
    gpio_set_level(PIN_BLE_EN, 0);
    gpio_set_level(PIN_EN_POW_ADAU, 0);
    gpio_set_level(PIN_EN_ADDR_LED, 1);
    gpio_set_level(PIN_FAN_1, 0);
    gpio_set_level(PIN_FAN_2, 0);

    vTaskDelay(pdMS_TO_TICKS(2000));

    display_init();
    buttons_init();
    leds_init();

    // Запускаємо фонову задачу опитування температурних датчиків (стек 4096 байт)
    xTaskCreate(temperature_task, "temperature_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Готово! Перехід у фоновий режим.");

    // Головний цикл UI: працює на високій швидкості і не блокується шиною 1-Wire
    while (1) {
        menu_update(); 
        vTaskDelay(pdMS_TO_TICKS(20)); // ~50 FPS для плавної реакції інтерфейсу
    }
}