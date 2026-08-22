#include "ak5572.h"
#include "esp_log.h"

static const char *TAG = "AK5572";
static i2c_port_t adc_i2c_port = I2C_NUM_MAX;

static const uint8_t ak5572_filter_map[] = {
    [AK5572_FILTER_SHARP]    = 0x00,
    [AK5572_FILTER_SLOW]     = 0x01,
    [AK5572_FILTER_SD_SHARP] = 0x04,
    [AK5572_FILTER_SD_SLOW]  = 0x05 
};

esp_err_t ak5572_write_reg(uint8_t reg_addr, uint8_t data) {
    if (adc_i2c_port == I2C_NUM_MAX) return ESP_ERR_INVALID_ARG;
    uint8_t tx_buf[2] = { reg_addr, data };
    return i2c_master_write_to_device(adc_i2c_port, AK5572_ADC_ADDR, tx_buf, sizeof(tx_buf), pdMS_TO_TICKS(50));
}

esp_err_t ak5572_init(i2c_port_t i2c_num) {
    adc_i2c_port = i2c_num;
    ESP_LOGI(TAG, "Ініціалізація AK5572 на I2C порту %d", i2c_num);

    // Регістр 0x02: Audio Data Format
    // За даташитом DIF1=1, DIF0=1 означає 32-bit I2S
    // Значення: 0b00000011 = 0x03
    ak5572_write_reg(0x02, 0x03);
    ESP_LOGI(TAG, "ADC налаштовано: 32-bit I2S");

    return ESP_OK;
}

esp_err_t ak5572_set_stereo_mode(void) {
    // Регістр 0x01: Power Management 2
    // Знімаємо Reset каналів та ставимо Стерео (0x01)
    return ak5572_write_reg(0x01, 0x01);
}

esp_err_t ak5572_set_filter(ak5572_filter_t filter) {
    if (filter > AK5572_FILTER_SD_SLOW) return ESP_ERR_INVALID_ARG;
    return ak5572_write_reg(0x04, ak5572_filter_map[filter]);
}