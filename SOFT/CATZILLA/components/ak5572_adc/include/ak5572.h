#ifndef AK5572_H
#define AK5572_H

#include <stdint.h>
#include "driver/i2c_master.h"

#define ADC_FILTER_SHARP_ROLLOFF      0x00
#define ADC_FILTER_SHORT_DELAY_SHARP  0x01
#define ADC_FILTER_SLOW_ROLLOFF       0x02
#define ADC_FILTER_SHORT_DELAY_SLOW   0x03

// Ініціалізація та реєстрація чипа на шині
void ak5572_init(i2c_master_bus_handle_t bus_handle, uint8_t i2c_addr, uint8_t initial_filter);
void ak5572_start(void);
void ak5572_set_filter(uint8_t filter_bits);

#endif // AK5572_H