#include "adau1452.h"
#include "esp_log.h"
#include <string.h>

/* 
 * Зовнішні функції вашої шини I2C (реалізовані в main/i2c_bus.c)
 */
extern esp_err_t i2c_bus_write_bytes(uint8_t dev_addr, const uint8_t *data, size_t len);
extern esp_err_t i2c_bus_write_read_bytes(uint8_t dev_addr, const uint8_t *wr_data, size_t wr_len, uint8_t *rd_data, size_t rd_len);

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

esp_err_t adau1452_write_u32(uint16_t reg_addr, uint32_t value) {
    uint8_t payload[6];
    pack_be16(&payload[0], reg_addr);
    pack_be32(&payload[2], value);
    return i2c_bus_write_bytes(ADAU1452_I2C_ADDR, payload, sizeof(payload));
}

esp_err_t adau1452_read_u32(uint16_t reg_addr, uint32_t *out_value) {
    if (!out_value) return ESP_ERR_INVALID_ARG;

    uint8_t addr_buf[2];
    uint8_t rx_buf[4] = {0};
    pack_be16(addr_buf, reg_addr);

    esp_err_t ret = i2c_bus_write_read_bytes(
        ADAU1452_I2C_ADDR,
        addr_buf, sizeof(addr_buf),
        rx_buf, sizeof(rx_buf)
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

    ret = adau1452_write_u32(REG_SAFELOAD_TARGET_ADDR, (uint32_t)(target_addr - 1));
    if (ret != ESP_OK) return ret;

    return adau1452_write_u32(REG_SAFELOAD_NUM_WORDS, (uint32_t)word_count);
}