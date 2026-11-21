#ifndef AK4493_H
#define AK4493_H

#include <stdint.h>
#include "driver/i2c_master.h"

// Внутрішнє кодування бітів для зручного парсингу в .c файлі
#define AK_SD_BIT    0x20 // Для регістра 0x01
#define AK_SLOW_BIT  0x02 // Для регістра 0x02
#define AK_SSLOW_BIT 0x01 // Для регістра 0x05

// Типи цифрових фільтрів
#define DAC_FILTER_SHARP_ROLLOFF               0x00
#define DAC_FILTER_SLOW_ROLLOFF                (AK_SLOW_BIT)
#define DAC_FILTER_SHORT_DELAY_SHARP           (AK_SD_BIT)
#define DAC_FILTER_SHORT_DELAY_SLOW            (AK_SD_BIT | AK_SLOW_BIT)
#define DAC_FILTER_SUPER_SLOW                  (AK_SSLOW_BIT)
#define DAC_FILTER_LOW_DISPERSION_SHORT_DELAY  (AK_SD_BIT | AK_SSLOW_BIT)

// Ініціалізація 3-х ЦАПів
void ak4493_init_all(i2c_master_bus_handle_t bus_handle, const uint8_t *i2c_addrs, uint8_t initial_filter);
void ak4493_start_all(void);
void ak4493_set_filter_all(uint8_t filter_bits);

#endif // AK4493_H