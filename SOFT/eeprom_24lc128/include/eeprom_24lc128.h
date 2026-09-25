#ifndef EEPROM_24LC128_H
#define EEPROM_24LC128_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#define EEPROM_I2C_ADDR       0x50
#define EEPROM_PAGE_SIZE      64
#define SETTINGS_MAGIC        0x12345678

typedef struct __attribute__((packed)) {
    uint32_t magic;                 // Маркер ініціалізації (0x12345678)
    
    // Периферійні чипи
    uint8_t  ak5572_preset;         // Пресет АЦП
    uint8_t  ak4493_filter;         // Фільтр ЦАП

    // Параметри ADAU1452 DSP
    uint8_t  adau_main_volume;      // Загальна гучність
    uint8_t  adau_vol_ch[6];        // Масив гучності: [0]=FL, [1]=FR, [2]=C, [3]=SUB, [4]=RL, [5]=RR
    uint8_t  adau_distances[6];     // Дистанції до динаміків (наприклад, у кроках по 10 см, 0-255 = 0-25.5м)
    
    // Режими системи
    uint8_t  adau_eq_preset;        // Еквалайзер (6 режимів: 0..5)
    uint8_t  adau_surround_mode;    // Перемикач: 0 - Music, 1 - Surround
    uint8_t  sys_mode;              // Перемикач: 0 - 2.1, 1 - 5.1

    uint8_t  crc8;                  // Контрольна сума CRC8 усіх полів вище
} app_settings_t;

// Глобальні копії налаштувань для RAM та фонового порівняння
extern app_settings_t g_settings;
extern app_settings_t g_saved_settings;

esp_err_t eeprom_init(void);
uint8_t eeprom_calculate_crc8(const uint8_t *data, size_t length);
esp_err_t eeprom_read_bytes(uint16_t mem_addr, uint8_t *data, size_t len);
esp_err_t eeprom_write_bytes(uint16_t mem_addr, const uint8_t *data, size_t len);
esp_err_t eeprom_load_settings(app_settings_t *settings);
esp_err_t eeprom_save_settings(const app_settings_t *settings);

// Нова функція для схеми з ESP-NOW (розумне збереження)
bool settings_commit_check(void);

#endif // EEPROM_24LC128_H