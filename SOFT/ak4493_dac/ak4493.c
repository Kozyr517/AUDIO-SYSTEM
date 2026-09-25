#include "ak4493.h"
#include "audio_config.h"
#include "i2c_bus.h"

static const uint8_t DAC_ADDRS[3] = {AK4493_DAC_ADDR_1, AK4493_DAC_ADDR_2, AK4493_DAC_ADDR_3};

void ak4493_init_soft_reset(uint8_t initial_filter_bits) {
    for (int i = 0; i < 3; i++) {
        uint8_t addr = DAC_ADDRS[i];
        i2c_bus_write_reg(addr, 0x00, 0x9C);             
        i2c_bus_write_reg(addr, 0x01, initial_filter_bits); 
        i2c_bus_write_reg(addr, 0x02, 0x00);             
        i2c_bus_write_reg(addr, 0x03, 0xFF);             
        i2c_bus_write_reg(addr, 0x04, 0xFF);             
        i2c_bus_write_reg(addr, 0x07, 0x04);             
    }
}

void ak4493_start(void) {
    for (int i = 0; i < 3; i++) {
        i2c_bus_write_reg(DAC_ADDRS[i], 0x00, 0x9D);     
    }
}

void ak4493_set_filter(uint8_t filter_bits) {
    for (int i = 0; i < 3; i++) {
        i2c_bus_write_reg(DAC_ADDRS[i], 0x01, filter_bits);
    }
}