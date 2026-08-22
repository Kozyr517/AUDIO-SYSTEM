#ifndef AK_PRESETS_H
#define AK_PRESETS_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c.h"
#include "ak4493.h"
#include "ak5572.h"

typedef struct {
    const char* name;
    ak4493_filter_t dac_filter;
    ak5572_filter_t adc_filter;
} ak_preset_t;

// Передаємо порт I2C під час ініціалізації!
esp_err_t ak_presets_init(i2c_port_t i2c_num);
void ak_presets_apply_by_idx(uint8_t preset_idx);
uint8_t ak_presets_get_count(void);
const char* ak_presets_get_name(uint8_t preset_idx);

#endif // AK_PRESETS_H