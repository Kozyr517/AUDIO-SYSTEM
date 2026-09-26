#ifndef AK4493_H
#define AK4493_H

#include <stdint.h>
#include "driver/i2c_master.h"

#define DAC_FILTER_SHARP_ROLLOFF               0x00
#define DAC_FILTER_SLOW_ROLLOFF                0x04
#define DAC_FILTER_SHORT_DELAY_SHARP           0x20
#define DAC_FILTER_SHORT_DELAY_SLOW            0x24
#define DAC_FILTER_SUPER_SLOW                  0x01
#define DAC_FILTER_LOW_DISPERSION_SHORT_DELAY  0x21

// Приймаємо масив із 3-х I2C адрес
void ak4493_init_all(i2c_master_bus_handle_t bus_handle, const uint8_t *i2c_addrs, uint8_t initial_filter);
void ak4493_start_all(void);
void ak4493_set_filter_all(uint8_t filter_bits);

#endif // AK4493_H