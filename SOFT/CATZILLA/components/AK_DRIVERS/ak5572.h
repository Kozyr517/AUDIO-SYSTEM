#ifndef AK5572_H
#define AK5572_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c.h"

#define AK5572_ADC_ADDR     0x13

typedef enum {
    AK5572_FILTER_SHARP = 0,
    AK5572_FILTER_SLOW,
    AK5572_FILTER_SD_SHARP,
    AK5572_FILTER_SD_SLOW
} ak5572_filter_t;

esp_err_t ak5572_init(i2c_port_t i2c_num);
esp_err_t ak5572_write_reg(uint8_t reg_addr, uint8_t data);
esp_err_t ak5572_set_stereo_mode(void);
esp_err_t ak5572_set_filter(ak5572_filter_t filter);

#endif // AK5572_H