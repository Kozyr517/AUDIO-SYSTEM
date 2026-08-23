#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "ds18b20.h"
#include "ui_app.h"
#include "lcd.h"

// ==================== ПІНИ ЖИВЛЕННЯ ТА КЕРУВАННЯ ====================
#define PIN_VSYS_EN         GPIO_NUM_4
#define PIN_BLE_EN          GPIO_NUM_5
#define PIN_EN_POW_ADAU     GPIO_NUM_10
#define PIN_EN_ALL_POWER    GPIO_NUM_21
#define PIN_EN_ADDR_LED     GPIO_NUM_47

#define PIN_FAN_1           GPIO_NUM_14
#define PIN_FAN_2           GPIO_NUM_13
#define PIN_DS18B20         GPIO_NUM_15

// Піни для I2C (зарезервовані для майбутньої ініціалізації)
#define I2C_MASTER_SCL      GPIO_NUM_6
#define I2C_MASTER_SDA      GPIO_NUM_7

static const char *TAG = "MAIN";

// ==================== СТРУКТУРА ТА АДРЕСИ ДАТЧИКІВ ====================
typedef struct {
    const char *name;
    uint8_t addr[8];
    float temp;
    bool valid;
} sensor_t;

sensor_t g_sensors[] = {
    { .name = "Силова камера",    .addr = {0x28, 0x08, 0x8E, 0x9C, 0x00, 0x00, 0x00, 0x25}, .temp = 0.0f, .valid = false },
    { .name = "Радіатор 1",       .addr = {0x28, 0xA3, 0xCB, 0x9C, 0x00, 0x00, 0x00, 0xBA}, .temp = 0.0f, .valid = false },
    { .name = "Радіатор 2",       .addr = {0x28, 0x90, 0x78, 0x9C, 0x00, 0x00, 0x00, 0x0F}, .temp = 0.0f, .valid = false },
    { .name = "Камера ADAU",      .addr = {0x28, 0xE6, 0xC4, 0x9C, 0x00, 0x00, 0x00, 0x4B}, .temp = 0.0f, .valid = false },
};

#define SENSOR_COUNT (sizeof(g_sensors) / sizeof(g_sensors[0]))

// ==================== ФУНКЦІЇ ОХОЛОДЖЕННЯ ====================
static void fans_pwm_init(void) {
    ledc_timer_config_t fan_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_10_BIT,
        .freq_hz          = 30,
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

static uint32_t calculate_fan_duty(float chamber_temp, float amp_temp_1, float amp_temp_2) {
    float amp_max = (amp_temp_1 > amp_temp_2) ? amp_temp_1 : amp_temp_2;
    float control_temp = (chamber_temp > amp_max) ? chamber_temp : amp_max;

    if (control_temp < 40.0f) return 0; 
    if (control_temp >= 70.0f) return 1023; 

    float duty_f = 153.0f + (control_temp - 40.0f) / (70.0f - 40.0f) * (1023.0f - 153.0f);
    return (uint32_t)duty_f;
}

static void temperature_task(void *pvParameters) {
    ds18b20_init(PIN_DS18B20);
    fans_pwm_init();

    while (1) {
        ds18b20_request_temperature(PIN_DS18B20); 
        vTaskDelay(pdMS_TO_TICKS(800)); 

        for (int i = 0; i < SENSOR_COUNT; i++) {
            ds18b20_read_temperature_addr(PIN_DS18B20, g_sensors[i].addr, &g_sensors[i].temp);
        }

        uint32_t duty_fan_1 = calculate_fan_duty(g_sensors[0].temp, g_sensors[1].temp, g_sensors[2].temp);
        uint32_t duty_fan_2 = calculate_fan_duty(g_sensors[3].temp, g_sensors[1].temp, g_sensors[2].temp);

        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty_fan_1);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty_fan_2);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);

        vTaskDelay(pdMS_TO_TICKS(2000)); 
    }
}

// ==================== MAIN ====================
void app_main(void) {
    ESP_LOGI(TAG, "=== СТАРТ СИСТЕМИ CATZILLA ===");

    // 1. НАЛАШТУВАННЯ ПІНІВ ЖИВЛЕННЯ (Без піна RS екрану, він ініціалізується в драйвері дисплея)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_EN_ALL_POWER) | (1ULL << PIN_VSYS_EN) |
                        (1ULL << PIN_BLE_EN) | (1ULL << PIN_EN_POW_ADAU) |
                        (1ULL << PIN_EN_ADDR_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // 2. ВМИКАЄМО ЖИВЛЕННЯ
    gpio_set_level(PIN_EN_ALL_POWER, 1);
    gpio_set_level(PIN_VSYS_EN, 1);
    gpio_set_level(PIN_BLE_EN, 0);
    gpio_set_level(PIN_EN_POW_ADAU, 0);
    gpio_set_level(PIN_EN_ADDR_LED, 1);

    // 3. АПАРАТНА ЗАТРИМКА СТАБІЛІЗАЦІЇ НАПРУГ
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 4. ІНІЦІАЛІЗАЦІЯ ДИСПЛЕЯ (SPI + Driver)
    lcd_bus_init();
    lcd_init();

    // 5. ЗАПУСК UI ТА АВТОМАТА СТАНІВ (із файлу ui_app.c)
    ui_app_init();

    // 6. ЗАПУСК ТЕРМОКОНТРОЛЮ
    xTaskCreate(temperature_task, "temperature_task", 4096, NULL, 5, NULL);

    // Видаляємо стартову задачу, оскільки всі процеси тепер крутяться у FreeRTOS тасках
    vTaskDelete(NULL);
}