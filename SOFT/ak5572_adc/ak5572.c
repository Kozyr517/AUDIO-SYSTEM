#include "ak5572.h"
#include "audio_config.h"
#include "i2c_bus.h"

void ak5572_init_soft_reset(uint8_t initial_filter_bits) {
    i2c_bus_write_reg(AK5572_ADC_ADDR, 0x00, 0x0E);             
    i2c_bus_write_reg(AK5572_ADC_ADDR, 0x01, 0x01);             
    i2c_bus_write_reg(AK5572_ADC_ADDR, 0x02, (initial_filter_bits & 0x03) | 0x80); 
    i2c_bus_write_reg(AK5572_ADC_ADDR, 0x03, 0x00);             
}

void ak5572_start(void) {
    i2c_bus_write_reg(AK5572_ADC_ADDR, 0x00, 0x0F);             
}

void ak5572_set_filter(uint8_t filter_bits) {
    i2c_bus_write_reg(AK5572_ADC_ADDR, 0x02, (filter_bits & 0x03) | 0x80);
}