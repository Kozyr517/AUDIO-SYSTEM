#ifndef AK5572_H
#define AK5572_H

#include <stdint.h>

#define ADC_FILTER_SHARP_ROLLOFF               0x00
#define ADC_FILTER_SHORT_DELAY_SHARP           0x01
#define ADC_FILTER_SLOW_ROLLOFF                0x02
#define ADC_FILTER_SHORT_DELAY_SLOW            0x03

void ak5572_init_soft_reset(uint8_t initial_filter_bits);
void ak5572_start(void);
void ak5572_set_filter(uint8_t filter_bits);

#endif // AK5572_H