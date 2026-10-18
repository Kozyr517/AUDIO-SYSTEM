#ifndef AK5572_H
#define AK5572_H

#include <stdint.h>
#include "driver/i2c_master.h"

// Виправлені типи цифрових фільтрів (Біти SD та SLOW у регістрі 0x04)
#define ADC_FILTER_SHARP_ROLLOFF           0x00 // SD=0, SLOW=0
#define ADC_FILTER_SLOW_ROLLOFF            0x01 // SD=0, SLOW=1
#define ADC_FILTER_SHORT_DELAY_SHARP       0x02 // SD=1, SLOW=0
#define ADC_FILTER_SHORT_DELAY_SLOW        0x03 // SD=1, SLOW=1

// Ініціалізація та реєстрація чипа на шині
void ak5572_init(i2c_master_bus_handle_t bus_handle, uint8_t i2c_addr, uint8_t initial_filter);

// Запуск АЦП (знімає Reset, подає живлення на канали)
void ak5572_start(void);

// Динамічна зміна цифрового фільтра
void ak5572_set_filter(uint8_t filter_bits);

#endif // AK5572_H