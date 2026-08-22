#ifndef AK4493_H
#define AK4493_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c.h"

// I2C адреси трьох мікросхем AK4493
#define AK4493_DAC1_ADDR    0x10
#define AK4493_DAC2_ADDR    0x11
#define AK4493_DAC3_ADDR    0x12
#define AK4493_COUNT        3

typedef enum {
    AK4493_FILTER_SILK = 0,
    AK4493_FILTER_PURRITY,
    AK4493_FILTER_DRIVE,
    AK4493_FILTER_ATMOS,
    AK4493_FILTER_VILVET,
    AK4493_FILTER_DIRECT
} ak4493_filter_t;

// Ініціалізація (передаємо порт I2C)
esp_err_t ak4493_init(i2c_port_t i2c_num);

// Функції керування
esp_err_t ak4493_write_reg(uint8_t dac_idx, uint8_t reg_addr, uint8_t data);
esp_err_t ak4493_set_filter(uint8_t dac_idx, ak4493_filter_t filter);
esp_err_t ak4493_set_filter_all(ak4493_filter_t filter);

#endif // AK4493_H