#include "ak4493.h"
#include "esp_log.h"

static const char *TAG = "AK4493";
static i2c_port_t dac_i2c_port = I2C_NUM_MAX; // Зберігаємо номер I2C порту
static const uint8_t dac_addrs[AK4493_COUNT] = { AK4493_DAC1_ADDR, AK4493_DAC2_ADDR, AK4493_DAC3_ADDR };

// Карта фільтрів (Регістр 0x02)
static const uint8_t ak4493_filter_map[] = {
    [AK4493_FILTER_SILK]    = 0x00, 
    [AK4493_FILTER_PURRITY] = 0x01, 
    [AK4493_FILTER_DRIVE]   = 0x02, 
    [AK4493_FILTER_ATMOS]   = 0x03, 
    [AK4493_FILTER_VILVET]  = 0x04, 
    [AK4493_FILTER_DIRECT]  = 0x06  
};

esp_err_t ak4493_write_reg(uint8_t dac_idx, uint8_t reg_addr, uint8_t data) {
    if (dac_idx >= AK4493_COUNT || dac_i2c_port == I2C_NUM_MAX) return ESP_ERR_INVALID_ARG;
    
    uint8_t tx_buf[2] = { reg_addr, data };
    return i2c_master_write_to_device(dac_i2c_port, dac_addrs[dac_idx], tx_buf, sizeof(tx_buf), pdMS_TO_TICKS(50));
}

esp_err_t ak4493_init(i2c_port_t i2c_num) {
    dac_i2c_port = i2c_num;
    ESP_LOGI(TAG, "Драйвер AK4493 підключено до I2C порту %d", i2c_num);

    // ТУТ БУДЕ СТАРТОВА ІНІЦІАЛІЗАЦІЯ (формат I2S, Power Down, Клоки)
    // Чекаю на ваші відповіді, щоб дописати ці команди.
    
    return ESP_OK;
}

esp_err_t ak4493_set_filter(uint8_t dac_idx, ak4493_filter_t filter) {
    if (filter > AK4493_FILTER_DIRECT) return ESP_ERR_INVALID_ARG;
    uint8_t reg_val = ak4493_filter_map[filter];
    
    esp_err_t ret = ak4493_write_reg(dac_idx, 0x02, reg_val);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "DAC #%d -> Фільтр встановлено (Рег 0x02 = 0x%02X)", dac_idx + 1, reg_val);
    } else {
        ESP_LOGE(TAG, "Помилка I2C при записі в DAC #%d", dac_idx + 1);
    }
    return ret;
}

esp_err_t ak4493_set_filter_all(ak4493_filter_t filter) {
    esp_err_t ret = ESP_OK;
    for (int i = 0; i < AK4493_COUNT; i++) {
        if (ak4493_set_filter(i, filter) != ESP_OK) ret = ESP_FAIL;
    }
    return ret;
}