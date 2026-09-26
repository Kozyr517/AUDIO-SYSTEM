#include "audio_manager.h"
#include "eeprom_24lc128.h" 
#include "i2c_bus.h"        // Беремо глобальну шину
#include "ak5572.h"         // Універсальний драйвер АЦП
#include "ak4493.h"         // Універсальний драйвер ЦАП
#include "esp_log.h"

static const char *TAG = "AUDIO_MGR";

// Масив адрес трьох ЦАПів (з оновленого .h файлу)
static const uint8_t dac_addrs[3] = {AK4493_DAC_ADDR_1, AK4493_DAC_ADDR_2, AK4493_DAC_ADDR_3};

// Карта фільтрів АЦП AK5572 (4 апаратні фільтри розтягнуті на 6 пресетів меню)
static const uint8_t ak5572_filter_map[6] = {
    ADC_FILTER_SHARP_ROLLOFF,        // [0] SILK
    ADC_FILTER_SLOW_ROLLOFF,         // [1] PURRITY
    ADC_FILTER_SHORT_DELAY_SHARP,    // [2] DRIVE
    ADC_FILTER_SHORT_DELAY_SLOW,     // [3] ATMOS
    ADC_FILTER_SLOW_ROLLOFF,         // [4] VILVET  
    ADC_FILTER_SHARP_ROLLOFF         // [5] DIRECT  
};

// Карта фільтрів ЦАП AK4493 (6 апаратних фільтрів = 6 пресетів меню)
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

    // 1. Оновлюємо АЦП (AK5572)
    uint8_t adc_filter = ak5572_filter_map[ui_filter_index];
    // Викликаємо тільки з одним аргументом - самим фільтром
    ak5572_set_filter(adc_filter); 
    
    // 2. Оновлюємо всі три ЦАПи (AK4493)
    uint8_t dac_filter = ak4493_filter_map[ui_filter_index];
    // Викликаємо тільки з одним аргументом
    ak4493_set_filter_all(dac_filter); 

    ESP_LOGI(TAG, "Фільтри оновлено. UI Пресет: %d (AK5572: 0x%02X, AK4493: 0x%02X)", 
             ui_filter_index, adc_filter, dac_filter);
}

void audio_manager_init_all(void) {
    ESP_LOGI(TAG, "Ініціалізація аудіочипів...");

    // Читаємо індекс обраного фільтра з EEPROM-структури
    uint8_t saved_filter_idx = g_settings.ak4493_filter;

    // Ініціалізація АЦП AK5572 (Тут передаємо шину та адресу, бо це перше знайомство)
    ak5572_init(i2c_bus_handle, AK5572_ADC_ADDR, ak5572_filter_map[saved_filter_idx]);
    // Старт викликається без аргументів!
    ak5572_start();                             

    // Ініціалізація трьох ЦАПів AK4493 (Тут передаємо шину та масив адрес)
    ak4493_init_all(i2c_bus_handle, dac_addrs, ak4493_filter_map[saved_filter_idx]);
    // Старт викликається без аргументів!
    ak4493_start_all();

    // TODO: ADAU1452 Init

    ESP_LOGI(TAG, "Аудіотракт готовий! Завантажено пресет меню: %d", saved_filter_idx);
}
    // TODO: ADAU1452 Init