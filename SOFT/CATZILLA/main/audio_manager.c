#include "audio_manager.h"
#include "eeprom_24lc128.h" 
#include "i2c_bus.h"        
#include "ak5572.h"         
#include "ak4493.h"         
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "AUDIO_MGR";

// Масив адрес трьох ЦАПів
static const uint8_t dac_addrs[3] = {AK4493_DAC_ADDR_1, AK4493_DAC_ADDR_2, AK4493_DAC_ADDR_3};

// Карти фільтрів (Ідеальне рішення для відв'язки UI від Hardware)
static const uint8_t ak5572_filter_map[6] = {
    ADC_FILTER_SHARP_ROLLOFF,        // [0] SILK
    ADC_FILTER_SLOW_ROLLOFF,         // [1] PURRITY
    ADC_FILTER_SHORT_DELAY_SHARP,    // [2] DRIVE
    ADC_FILTER_SHORT_DELAY_SLOW,     // [3] ATMOS
    ADC_FILTER_SLOW_ROLLOFF,         // [4] VILVET  
    ADC_FILTER_SHARP_ROLLOFF         // [5] DIRECT  
};

static const uint8_t ak4493_filter_map[6] = {
    DAC_FILTER_LOW_DISPERSION_SHORT_DELAY, // [0] SILK
    DAC_FILTER_SHORT_DELAY_SHARP,          // [1] PURRITY
    DAC_FILTER_SHARP_ROLLOFF,              // [2] DRIVE
    DAC_FILTER_SLOW_ROLLOFF,               // [3] ATMOS
    DAC_FILTER_SUPER_SLOW,                 // [4] VILVET
    DAC_FILTER_SHORT_DELAY_SLOW            // [5] DIRECT
};

void audio_update_filters(uint8_t ui_filter_index) {
    if (ui_filter_index > 5) return;

    ESP_LOGI(TAG, "Перемикання на пресет: %d. Активація Mute...", ui_filter_index);

    // 1. Приглушуємо звук перед зміною математики фільтрів (запобігає клацанню)
    // TODO: adau1452_set_master_mute(true); 
    vTaskDelay(pdMS_TO_TICKS(10)); // Даємо 10 мс на спад амплітуди

    // 2. Оновлюємо регістри
    uint8_t adc_filter = ak5572_filter_map[ui_filter_index];
    ak5572_set_filter(adc_filter); 
    
    uint8_t dac_filter = ak4493_filter_map[ui_filter_index];
    ak4493_set_filter_all(dac_filter); 

    // 3. Повертаємо звук
    vTaskDelay(pdMS_TO_TICKS(5)); // Даємо час ЦАПам зафіксувати нові налаштування
    // TODO: adau1452_set_master_mute(false);

    ESP_LOGI(TAG, "Фільтри оновлено (AK5572: 0x%02X, AK4493: 0x%02X)", adc_filter, dac_filter);
}

void audio_manager_init_all(void) {
    ESP_LOGI(TAG, "Ініціалізація аудіотракту...");

    // Читаємо індекс обраного фільтра (перейменував на audio_preset для логіки)
    uint8_t saved_filter_idx = g_settings.audio_preset; 
    if (saved_filter_idx > 5) saved_filter_idx = 0; // Захист від сміття в EEPROM

    // ==============================================================
    // КРОК 1: Ініціалізація I2C AKM (Чипи залишаються в стані Reset)
    // ==============================================================
    ESP_LOGI(TAG, "Налаштування регістрів АЦП/ЦАП...");
    ak5572_init(i2c_bus_handle, AK5572_ADC_ADDR, ak5572_filter_map[saved_filter_idx]);
    ak4493_init_all(i2c_bus_handle, dac_addrs, ak4493_filter_map[saved_filter_idx]);

    // ==============================================================
    // КРОК 2: Запуск DSP (Генерація MCLK, BCLK, LRCLK)
    // ==============================================================
    ESP_LOGI(TAG, "Запуск DSP ADAU1452...");
    // TODO: adau1452_init(i2c_bus_handle, ADAU1452_DSP_ADDR); 
    
    // Чекаємо стабілізації PLL всередині DSP
    vTaskDelay(pdMS_TO_TICKS(50)); 

    // ==============================================================
    // КРОК 3: Зняття Reset з AKM (Клоки вже присутні!)
    // ==============================================================
    ESP_LOGI(TAG, "Клоки стабільні. Запуск модуляторів АЦП/ЦАП...");
    ak5572_start();                                 
    ak4493_start_all();

    ESP_LOGI(TAG, "Аудіотракт CATZILLA SYSTEM успішно запущено!");
}