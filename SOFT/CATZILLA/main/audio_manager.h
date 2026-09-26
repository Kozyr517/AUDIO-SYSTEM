#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <stdint.h>

// Фізичні I2C адреси мікросхем на платі
#define AK4493_DAC_ADDR_1    0x10 // ЦАП 1
#define AK4493_DAC_ADDR_2    0x11 // ЦАП 2
#define AK4493_DAC_ADDR_3    0x12 // ЦАП 3
#define AK5572_ADC_ADDR      0x13 // АЦП
#define ADAU1452_DSP_ADDR    0x38 // DSP

// Прототипи функцій
void audio_manager_init_all(void);
void audio_update_filters(uint8_t ui_filter_index);

#endif // AUDIO_MANAGER_H 