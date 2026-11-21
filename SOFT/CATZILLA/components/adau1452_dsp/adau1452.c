#include "adau1452.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

static const char *TAG = "ADAU1452";
static i2c_master_dev_handle_t s_dev_handle = NULL;

#define ADAU1452_I2C_SPEED_HZ   400000
#define ADAU1452_I2C_TIMEOUT_MS 50

static inline void pack_be16(uint8_t *buf, uint16_t val) {
    buf[0] = (uint8_t)(val >> 8);
    buf[1] = (uint8_t)(val & 0xFF);
}

static inline void pack_be32(uint8_t *buf, uint32_t val) {
    buf[0] = (uint8_t)(val >> 24);
    buf[1] = (uint8_t)(val >> 16);
    buf[2] = (uint8_t)(val >> 8);
    buf[3] = (uint8_t)(val & 0xFF);
}

esp_err_t adau1452_init(i2c_master_bus_handle_t bus_handle, uint8_t dev_addr) {
    if (!bus_handle) return ESP_ERR_INVALID_ARG;
    if (s_dev_handle != NULL) return ESP_OK;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = dev_addr,
        .scl_speed_hz    = ADAU1452_I2C_SPEED_HZ,
    };

    esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &s_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Помилка додавання ADAU1452 (0x%02X): %s", dev_addr, esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_master_probe(bus_handle, dev_addr, ADAU1452_I2C_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ADAU1452 (0x%02X) не відповідає на шині: %s", dev_addr, esp_err_to_name(ret));
    }
    return ESP_OK;
}

esp_err_t adau1452_write_u32(uint16_t reg_addr, uint32_t value) {
    if (!s_dev_handle) return ESP_ERR_INVALID_STATE;

    uint8_t payload[6];
    pack_be16(&payload[0], reg_addr);
    pack_be32(&payload[2], value);

    return i2c_master_transmit(s_dev_handle, payload, sizeof(payload), ADAU1452_I2C_TIMEOUT_MS);
}

esp_err_t adau1452_read_u32(uint16_t reg_addr, uint32_t *out_value) {
    if (!s_dev_handle) return ESP_ERR_INVALID_STATE;
    if (!out_value) return ESP_ERR_INVALID_ARG;

    uint8_t addr_buf[2];
    uint8_t rx_buf[4] = {0};
    pack_be16(addr_buf, reg_addr);

    esp_err_t ret = i2c_master_transmit_receive(
        s_dev_handle,
        addr_buf, sizeof(addr_buf),
        rx_buf, sizeof(rx_buf),
        ADAU1452_I2C_TIMEOUT_MS
    );
    if (ret == ESP_OK) {
        *out_value = ((uint32_t)rx_buf[0] << 24) |
                     ((uint32_t)rx_buf[1] << 16) |
                     ((uint32_t)rx_buf[2] << 8)  |
                     ((uint32_t)rx_buf[3]);
    }
    return ret;
}

esp_err_t adau1452_safeload_write(uint16_t target_addr, const uint32_t *data, uint8_t word_count) {
    if (!data || word_count == 0 || word_count > 5) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret;
    for (uint8_t i = 0; i < word_count; i++) {
        ret = adau1452_write_u32(REG_SAFELOAD_DATA_0 + i, data[i]);
        if (ret != ESP_OK) return ret;
    }

    ret = adau1452_write_u32(REG_SAFELOAD_TARGET_ADDR, (uint32_t)target_addr);
    if (ret != ESP_OK) return ret;

    ret = adau1452_write_u32(REG_SAFELOAD_NUM_WORDS, (uint32_t)word_count);
    if (ret == ESP_OK) {
        esp_rom_delay_us(50);
    }
    return ret;
}

esp_err_t adau1452_set_slew_mux(uint16_t base_addr, uint8_t num_inputs, uint8_t active_idx) {
    if (num_inputs == 0 || num_inputs > 5 || active_idx >= num_inputs) {
        return ESP_ERR_INVALID_ARG;
    }

    for (uint8_t i = 0; i < num_inputs; i++) {
        uint32_t gain = (i == active_idx) ? DSP_UNITY_8_24 : DSP_ZERO_8_24;
        esp_err_t ret = adau1452_write_u32(base_addr + i, gain);
        if (ret != ESP_OK) return ret;
    }
    return ESP_OK;
}