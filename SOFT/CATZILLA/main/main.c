#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "menu.h"
#include "ds18b20.h"

// ==================== ПІНИ ЖИВЛЕННЯ ТА КЕРУВАННЯ ====================
#define PIN_VSYS_EN         GPIO_NUM_4
#define PIN_BLE_EN          GPIO_NUM_5
#define PIN_EN_POW_ADAU     GPIO_NUM_10
#define PIN_EN_ALL_POWER    GPIO_NUM_21
#define PIN_EN_ADDR_LED     GPIO_NUM_47

// Поміняно місцями піни вентиляторів за твоїм запитом
#define PIN_FAN_1           GPIO_NUM_14
#define PIN_FAN_2           GPIO_NUM_13

#define PIN_DS18B20         GPIO_NUM_15

static const char *TAG = "MAIN";

// ==================== СТРУКТУРА ТА АДРЕСИ ДАТЧИКІВ ====================
typedef struct {
    const char *name;
    uint8_t addr[8];
    float temp;
    bool valid;
} sensor_t;

// Усі 4 датчики із жорсткою прив'язкою до адрес
sensor_t g_sensors[] = {
    { .name = "Силова камера",    .addr = {0x28, 0x08, 0x8E, 0x9C, 0x00, 0x00, 0x00, 0x25}, .temp = 0.0f, .valid = false }, // 0
    { .name = "Радіатор 1",       .addr = {0x28, 0xA3, 0xCB, 0x9C, 0x00, 0x00, 0x00, 0xBA}, .temp = 0.0f, .valid = false }, // 1
    { .name = "Радіатор 2",       .addr = {0x28, 0x90, 0x78, 0x9C, 0x00, 0x00, 0x00, 0x0F}, .temp = 0.0f, .valid = false }, // 2
    { .name = "Камера ADAU",      .addr = {0x28, 0xE6, 0xC4, 0x9C, 0x00, 0x00, 0x00, 0x4B}, .temp = 0.0f, .valid = false }, // 3
};

#define SENSOR_COUNT (sizeof(g_sensors) / sizeof(g_sensors[0]))

// Ініціалізація PWM (LEDC) на 30 Гц з 10-бітною роздільною здатністю (0 - 1023)
static void fans_pwm_init(void) {
    ledc_timer_config_t fan_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_10_BIT, // 10 біт для коректної роботи 30 Гц
        .freq_hz          = 30,                // Частота 30 Гц
        .clk_cfg          = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&fan_timer);

    ledc_channel_config_t fan1_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PIN_FAN_1,
        .duty           = 0,
        .hpoint         = 0
    };
    ledc_channel_config(&fan1_channel);

    ledc_channel_config_t fan2_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_1,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PIN_FAN_2,
        .duty           = 0,
        .hpoint         = 0
    };
    ledc_channel_config(&fan2_channel);
}

// Розрахунок обертів для кожного кулера окремо з урахуванням пріоритету радіатора (100% при 70°C+)
static uint32_t calculate_fan_duty(float chamber_temp, float amp_temp_1, float amp_temp_2) {
    // Знаходимо найгарячішу точку на радіаторі
    float amp_max = (amp_temp_1 > amp_temp_2) ? amp_temp_1 : amp_temp_2;
    
    // Радіатор має вищий пріоритет (якщо він гарячіший за камеру, керуємо відносно нього)
    float control_temp = (chamber_temp > amp_max) ? chamber_temp : amp_max;

    // До 40°C охолодження повністю вимкнене (0%)
    if (control_temp < 40.0f) {
        return 0; 
    }
    
    // При досягненні 70°C і вище — 100% потужності (1023)
    if (control_temp >= 70.0f) {
        return 1023; 
    }

    // Плавний розгін від 40°C (153) до 70°C (1023)
    float duty_f = 153.0f + (control_temp - 40.0f) / (70.0f - 40.0f) * (1023.0f - 153.0f);
    return (uint32_t)duty_f;
}

// ==================== ФОНОВА ЗАДАЧА ТЕМПЕРАТУРИ ТА ОХОЛОДЖЕННЯ ====================
static void temperature_task(void *pvParameters) {
    ds18b20_init(PIN_DS18B20);
    fans_pwm_init();

    while (1) {
        // Одночасний запит температури у всіх датчиків на шині
        ds18b20_request_temperature(PIN_DS18B20); 
        vTaskDelay(pdMS_TO_TICKS(800)); // Час на конвертацію

        // Почергове зчитування всіх датчиків за їхніми адресами
        for (int i = 0; i < SENSOR_COUNT; i++) {
            esp_err_t err = ds18b20_read_temperature_addr(PIN_DS18B20, g_sensors[i].addr, &g_sensors[i].temp);
            g_sensors[i].valid = (err == ESP_OK);
        }

        // Логування всіх датчиків 
        /*
        ESP_LOGI(TAG, "ТЕМПЕРАТУРИ -> Силова: %.1f°C | Радіатор 1: %.1f°C | Радіатор 2: %.1f°C | ADAU: %.1f°C", 
                 g_sensors[0].temp, g_sensors[1].temp, g_sensors[2].temp, g_sensors[3].temp);
        */

        // Розрахунок шиму для Фан 1 (Силова камера + Радіатори)
        uint32_t duty_fan_1 = calculate_fan_duty(g_sensors[0].temp, g_sensors[1].temp, g_sensors[2].temp);
        
        // Розрахунок шиму для Фан 2 (ADAU камера + Радіатори)
        uint32_t duty_fan_2 = calculate_fan_duty(g_sensors[3].temp, g_sensors[1].temp, g_sensors[2].temp);

        // Застосування PWM на відповідні піни вентиляторів
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty_fan_1);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty_fan_2);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);

        // Логування поточного стану обох кулерів 
        /*
        ESP_LOGI(TAG, "ОБЕРТИ КУЛЕРІВ -> Фан 1: %lu (%lu%%) | Фан 2: %lu (%lu%%)", 
                 duty_fan_1, duty_fan_1 * 100 / 1023, duty_fan_2, duty_fan_2 * 100 / 1023);
        */

        vTaskDelay(pdMS_TO_TICKS(2000)); 
    }
}

// ==================== MAIN ====================
void app_main(void) {
    ESP_LOGI(TAG, "=== СТАРТ СИСТЕМИ CATZILLA ===");

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
    gpio_set_level(PIN_EN_POW_ADAU, 0);
    gpio_set_level(PIN_EN_ADDR_LED, 1);

    vTaskDelay(pdMS_TO_TICKS(2000));

    display_init();
    buttons_init();
    leds_init();

    // Запуск фонової задачі охолодження
    xTaskCreate(temperature_task, "temperature_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Готово! Система охолодження активована.");

    while (1) {
        menu_update(); 
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}