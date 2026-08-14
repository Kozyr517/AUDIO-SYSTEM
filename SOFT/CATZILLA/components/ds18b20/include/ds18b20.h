#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

// Команди 1-Wire для DS18B20
#define DS18B20_CMD_SKIP_ROM      0xCC
#define DS18B20_CMD_MATCH_ROM     0x55
#define DS18B20_CMD_SEARCH_ROM    0xF0
#define DS18B20_CMD_CONVERT_T     0x44
#define DS18B20_CMD_READ_SCRATCH  0xBE

/**
 * @brief Ініціалізація піна (режим Open-Drain)
 */
esp_err_t ds18b20_init(gpio_num_t pin);

/**
 * @brief Пошук усіх датчиків на шині
 * @param pin Пін 1-Wire
 * @param rom_addresses Масив для збереження знайдених адрес [max_devices][8]
 * @param max_devices Максимальна кількість датчиків, які можна знайти
 * @param found_count Вказівник на змінну, куди запишеться кількість реально знайдених датчиків
 */
esp_err_t ds18b20_search_devices(gpio_num_t pin, uint8_t rom_addresses[][8], uint8_t max_devices, uint8_t *found_count);

/**
 * @brief Запит на вимірювання температури для конкретного датчика за його адресою
 */
esp_err_t ds18b20_request_temperature_addr(gpio_num_t pin, const uint8_t *rom_code);

/**
 * @brief Зчитування температури для конкретного датчика за його адресою
 */
esp_err_t ds18b20_read_temperature_addr(gpio_num_t pin, const uint8_t *rom_code, float *temp);

// Старі функції (якщо треба для сумісності з 1 датчиком)
esp_err_t ds18b20_request_temperature(gpio_num_t pin);
esp_err_t ds18b20_read_temperature(gpio_num_t pin, float *temp);