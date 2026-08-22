#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

// ==================== ДИСПЛЕЙ ТА АНІМАЦІЇ ====================
#include "lcd.h"
#include "gp1247ai.h"
#include "animation.h"
#include "anim_cat1.h"
#include "menu.h"
#include "ds18b20.h"

// Вказуємо компілятору, що зміна lcd оголошена в lcd.c
extern TypeDef_GP1247AI lcd;

// ==================== ПІНИ ЖИВЛЕННЯ ТА КЕРУВАННЯ ====================
#define PIN_VSYS_EN         GPIO_NUM_4
#define PIN_BLE_EN          GPIO_NUM_5
#define PIN_EN_POW_ADAU     GPIO_NUM_10
#define PIN_EN_ALL_POWER    GPIO_NUM_21
#define PIN_EN_ADDR_LED     GPIO_NUM_47

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

sensor_t g_sensors[] = {
    { .name = "Силова камера",    .addr = {0x28, 0x08, 0x8E, 0x9C, 0x00, 0x00, 0x00, 0x25}, .temp = 0.0f, .valid = false }, 
    { .name = "Радіатор 1",       .addr = {0x28, 0xA3, 0xCB, 0x9C, 0x00, 0x00, 0x00, 0xBA}, .temp = 0.0f, .valid = false }, 
    { .name = "Радіатор 2",       .addr = {0x28, 0x90, 0x78, 0x9C, 0x00, 0x00, 0x00, 0x0F}, .temp = 0.0f, .valid = false }, 
    { .name = "Камера ADAU",      .addr = {0x28, 0xE6, 0xC4, 0x9C, 0x00, 0x00, 0x00, 0x4B}, .temp = 0.0f, .valid = false }, 
};

#define SENSOR_COUNT (sizeof(g_sensors) / sizeof(g_sensors[0]))

// Ініціалізація PWM (LEDC) на 30 Гц
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

// Розрахунок обертів кулера
static uint32_t calculate_fan_duty(float chamber_temp, float amp_temp_1, float amp_temp_2) {
    float amp_max = (amp_temp_1 > amp_temp_2) ? amp_temp_1 : amp_temp_2;
    float control_temp = (chamber_temp > amp_max) ? chamber_temp : amp_max;

    if (control_temp < 40.0f) {
        return 0; 
    }
    if (control_temp >= 70.0f) {
        return 1023; 
    }

    float duty_f = 153.0f + (control_temp - 40.0f) / (70.0f - 40.0f) * (1023.0f - 153.0f);
    return (uint32_t)duty_f;
}

// ==================== ФОНОВА ЗАДАЧА ТЕМПЕРАТУРИ ====================
static void temperature_task(void *pvParameters) {
    ds18b20_init(PIN_DS18B20);
    fans_pwm_init();

    while (1) {
        ds18b20_request_temperature(PIN_DS18B20); 
        vTaskDelay(pdMS_TO_TICKS(800));

        for (int i = 0; i < SENSOR_COUNT; i++) {
            esp_err_t err = ds18b20_read_temperature_addr(PIN_DS18B20, g_sensors[i].addr, &g_sensors[i].temp);
            g_sensors[i].valid = (err == ESP_OK);
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

// ==================== АНІМАЦІЯ ЗАВАНТАЖЕННЯ ====================
void draw_boot_animation(void) {
    const int16_t SCREEN_WIDTH = 256;
    const int16_t SCREEN_HEIGHT = 64;

    const int16_t cat_y = (SCREEN_HEIGHT - ANIM_CAT_1_H) / 2;
    const int16_t start_x = 0;
    const int16_t end_x = SCREEN_WIDTH - ANIM_CAT_1_W;
    const int16_t step_x = 4;
    const uint32_t delay_ms = 60;

    ESP_LOGI(TAG, "Запуск анімації кота...");

    LCD_clear(&lcd);
    LCD_Update(&lcd);

    uint8_t current_frame = 0;

    for (int16_t x = start_x; x <= end_x; x += step_x) {
        LCD_clear(&lcd);
        
        // Малюємо поточний кадр кота
        LCD_DrawBitmap(&lcd, x, cat_y, cat1_anim[current_frame], ANIM_CAT_1_W, ANIM_CAT_1_H, 1);
        LCD_Update(&lcd);

        vTaskDelay(pdMS_TO_TICKS(delay_ms));

        current_frame++;
        if (current_frame >= ANIM_CAT_1_FRAMES) {
            current_frame = 0;
        }
    }
}

// ==================== MAIN ====================
void app_main(void) {
    ESP_LOGI(TAG, "=== СТАРТ СИСТЕМИ CATZILLA ===");

    // 1. Налаштування пінів керування живленням
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

    // 2. Подаємо живлення: вмикаємо загальне живлення та живлення ADAU
    gpio_set_level(PIN_EN_ALL_POWER, 1);
    gpio_set_level(PIN_VSYS_EN, 1);
    gpio_set_level(PIN_BLE_EN, 0);
    gpio_set_level(PIN_EN_POW_ADAU, 1); // Живлення ADAU пішло!
    gpio_set_level(PIN_EN_ADDR_LED, 1);

    // 3. Ініціалізація шини дисплея та периферії
    lcd_bus_init();
    lcd_init();

    buttons_init();
    leds_init();

    // Ініціалізуємо чергу пресетів (поки без ініціалізації самих чіпів)
    ak_presets_init();

    // 4. Програвання анімації руху кота (у цей час живлення на платі стабілізується)
    draw_boot_animation();

    // 5. КОЛИ АНІМАЦІЯ ЗАВЕРШИЛАСЬ: робимо апаратний ресет і запускаємо аудіодрайвери
    // (Замість I2C_NUM_0 вкажи свій номер шини, якщо він інший, наприклад I2C_NUM_1)
    if (ak_audio_hw_start(I2C_NUM_0) != ESP_OK) {
        ESP_LOGE(TAG, "Помилка ініціалізації аудіомікросхем!");
    }

    // 6. Запуск фонової задачі збору температур і керування кулерами
    xTaskCreate(temperature_task, "temperature_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Готово! Система активована.");

    while (1) {
        menu_update(); 
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}