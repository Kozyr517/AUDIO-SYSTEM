#ifndef EEPROM_24LC128_H
#define EEPROM_24LC128_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#define EEPROM_I2C_ADDR       0x50
#define EEPROM_PAGE_SIZE      64

// Змінено магічне число для примусового перезапису налаштувань
#define SETTINGS_MAGIC        0xCA72111C 

// Головна структура налаштувань (Єдине джерело правди)
typedef struct __attribute__((packed)) {
    uint32_t magic;                 // Маркер ініціалізації

    // Периферійні чипи
    uint8_t  ak5572_preset;         // Пресет АЦП (для всіх 3-х чипів)
    uint8_t  ak4493_filter;         // Фільтр ЦАП (для всіх 3-х чипів)

    // Параметри ADAU1452 DSP
    uint8_t  adau_main_volume;      // Загальна гучність (0-100 або 0-255)
    uint8_t  adau_vol_ch[6];        // Масив гучності зі зміщенням: 24 = 0dB. [0]=FL, [1]=FR...
    uint8_t  adau_distances[6];     // Дистанції до динаміків (у кроках по 10 см, 0-255 = 0-25.5м)
    
    // Режими системи
    uint8_t  adau_eq_preset;        // Еквалайзер (6 режимів: 0..5)
    uint8_t  adau_surround_mode;    // Перемикач: 0 - Music, 1 - Surround
    uint8_t  sys_mode;              // Перемикач: 0 - 2.1, 1 - 5.1

    uint8_t  crc8;                  // Контрольна сума CRC8 усіх полів вище
} app_settings_t;

// Глобальні копії налаштувань для RAM та фонового порівняння
extern app_settings_t g_settings;
extern app_settings_t g_saved_settings;

// Базові функції
esp_err_t eeprom_init(i2c_master_bus_handle_t bus_handle);
uint8_t eeprom_calculate_crc8(const uint8_t *data, size_t length);
esp_err_t eeprom_read_bytes(uint16_t mem_addr, uint8_t *data, size_t len);
esp_err_t eeprom_write_bytes(uint16_t mem_addr, const uint8_t *data, size_t len);

// Високорівневі функції
esp_err_t eeprom_load_settings(app_settings_t *settings);
esp_err_t eeprom_save_settings(const app_settings_t *settings);

// Функція для автоматичного збереження при змінах
bool settings_commit_check(void);

#endif // EEPROM_24LC128_H