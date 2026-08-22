#include "dsp.h"

void dsp_init(void) {
    // Пустишка для відладки
}

void set_input_mux(uint16_t mux) {
    (void)mux;
}

void set_eq_mux(uint16_t mux) {
    (void)mux;
}

void set_mute_left(uint16_t mute) {
    (void)mute;
}

void set_mute_right(uint16_t mute) {
    (void)mute;
}

void set_volume(uint8_t vol) {
    (void)vol;
}

void set_eq(TypedefEQFreq freq, int8_t var) {
    (void)freq;
    (void)var;
}


/* =========================================================================
   СТРИРИЙ КОД (ЗАКОМІЧЕНИЙ НА МАЙБУТНЄ):
   =========================================================================

#include "dsp.h"

i2c_master_bus_handle_t g_i2c_master_bus_handle;
i2c_master_dev_handle_t i2c_dsp_handle;

static void to_fixed_float(uint8_t *p_dest, float var) {
    int32_t fixed_f = (int32_t)(var * 0x1000000);

    p_dest[0] = (uint8_t)(fixed_f >> 24);
    p_dest[1] = (uint8_t)(fixed_f >> 16);
    p_dest[2] = (uint8_t)(fixed_f >> 8);
    p_dest[3] = (uint8_t)(fixed_f);
}

static void write_reg(const uint8_t *buff, size_t size) { ESP_ERROR_CHECK(i2c_master_transmit(i2c_dsp_handle, buff, size, 10)); }

void old_dsp_init(void) {
    i2c_master_bus_config_t i2c_master_bus = {.scl_io_num = I2C_MASTER_SCL_PIN,
                                              .sda_io_num = I2C_MASTER_SDA_PIN,
                                              .i2c_port = I2C_NUM_0,
                                              .clk_source = I2C_CLK_SRC_DEFAULT,
                                              .flags.enable_internal_pullup = true};

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_master_bus, &g_i2c_master_bus_handle));

    const i2c_device_config_t dev_config = {.device_address = DSP_ADR, .scl_speed_hz = 100000, .dev_addr_length = I2C_ADDR_BIT_LEN_7};

    ESP_ERROR_CHECK(i2c_master_bus_add_device(g_i2c_master_bus_handle, &dev_config, &i2c_dsp_handle));
}

void old_set_input_mux(uint16_t mux) {
    uint8_t temp[6] = {DSP_INPUT_MUX_ADR, 0x00, 0x00, 0x00, 0x00};

    if (mux == USB) {
        to_fixed_float(temp + 2, 0.0);
    } else {
        to_fixed_float(temp + 2, 1.1920928955078125e-7);
    }
    write_reg(temp, 6);
}

void old_set_eq_mux(uint16_t mux) {
    uint8_t temp[6] = {DSP_EQ_MUX_ADR, 0x00, 0x00, 0x00, 0x00};

    if (mux == EQ_ON) {
        to_fixed_float(temp + 2, 0.0);
    } else {
        to_fixed_float(temp + 2, 1.1920928955078125e-7);
    }
    write_reg(temp, 6);
}

void old_set_mute_left(uint16_t mute) {
    uint8_t temp[6] = {DSP_MUTE_L_ADR, 0x00, 0x00, 0x00, 0x00};

    if (mute) {
        to_fixed_float(temp + 2, 0.0);
    } else {
        to_fixed_float(temp + 2, 1.0);
    }
    write_reg(temp, 6);
}

void old_set_mute_right(uint16_t mute) {
    uint8_t temp[6] = {DSP_MUTE_R_ADR, 0x00, 0x00, 0x00, 0x00};

    if (mute) {
        to_fixed_float(temp + 2, 0.0);
    } else {
        to_fixed_float(temp + 2, 1.0);
    }
    write_reg(temp, 6);
}

void old_set_volume(uint8_t vol) {
    if (vol > 63) {
        vol = 63;
    }
    write_reg(EQ_VOL[vol], 6);
}

void old_set_eq(TypedefEQFreq freq, int8_t var) {
    if (freq > hz_16000) {
        freq = hz_16000;
    }
    if (var > 15) {
        var = 15;
    }
    if (var < -15) {
        var = -15;
    }
    uint8_t set = var + 15;
    write_reg(EQ_DATA[freq][set], 22);
    write_reg(EQ_CMD[freq], 10);
}
========================================================================= */