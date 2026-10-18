#ifndef AK4493_H
#define AK4493_H

#include <stdint.h>

#define DAC_FILTER_SHARP_ROLLOFF               0x00
#define DAC_FILTER_SLOW_ROLLOFF                0x04
#define DAC_FILTER_SHORT_DELAY_SHARP           0x20
#define DAC_FILTER_SHORT_DELAY_SLOW            0x24
#define DAC_FILTER_SUPER_SLOW                  0x01
#define DAC_FILTER_LOW_DISPERSION_SHORT_DELAY  0x21

void ak4493_init_soft_reset(uint8_t initial_filter_bits);
void ak4493_start(void);
void ak4493_set_filter(uint8_t filter_bits);

#endif // AK4493_H