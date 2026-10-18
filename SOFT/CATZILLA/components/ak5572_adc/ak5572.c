#include "ak5572.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "AK5572";

// Локальний хендл пристрою на шині I2C
static i2c_master_dev_handle_t dev_handle = NULL;

// Внутрішня функція запису з механізмом повторення (до 3 спроб)
static esp_err_t write_reg(uint8_t reg, uint8_t data) {
    if (dev_handle == NULL) {
        ESP_LOGE(TAG, "Пристрій I2C не ініціалізовано");
        return ESP_ERR_INVALID_STATE;
    }
    
    uint8_t buf[2] = {reg, data};
    esp_err_t err = ESP_FAIL;
    uint8_t retries = 3;
    
    while (retries--) {
        err = i2c_master_transmit(dev_handle, buf, 2, -1);
        if (err == ESP_OK) return ESP_OK;
        vTaskDelay(pdMS_TO_TICKS(5)); 
    }
    
    ESP_LOGE(TAG, "Помилка I2C (Регістр 0x%02X): %s", reg, esp_err_to_name(err));
    return err;
}

void ak5572_init(i2c_master_bus_handle_t bus_handle, uint8_t i2c_addr, uint8_t initial_filter) {
    // 1. Реєстрація пристрою на шині I2C
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = i2c_addr,
        .scl_speed_hz = 400000,
    };
    esp_err_t err = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Не вдалося додати AK5572 на шину I2C");
        return;
    }

    // ==========================================
    // БЛОК 1: ЗУПИНКА ТА ПІДГОТОВКА (RSTN = 0)
    // ==========================================
    // Reg 0x00: Вимикаємо живлення модуляторів CH1 та CH2 (біти 1:0 = 00). 
    // Зарезервовані старші біти (7:2) ЗАВЖДИ повинні бути '1'.
    write_reg(0x00, 0xFC); 
    
    // Reg 0x01: Утримуємо чип в Reset (RSTN = 0) на час налаштування. Стерео режим.
    write_reg(0x01, 0x00);

    // ==========================================
    // БЛОК 2: КОНФІГУРАЦІЯ АУДІО ТА ФІЛЬТРІВ
    // ==========================================
    // Reg 0x02 (Control 1):
    // Біти 6:3 (CKS)  = 0000 (Режим Slave, MCLK 128fs -> Quad Speed 192kHz)
    // Біти 2:1 (DIF)  = 11   (Формат 32-bit I2S)
    // Біт 0    (HPFE) = 1    (Увімкнено High Pass Filter для зрізу DC-складової)
    // Разом: 0000 0111 = 0x07
    write_reg(0x02, 0x07);
    
    // Reg 0x03 (Control 2): 
    // TDM[1:0] = 00 (Нормальний режим I2S, без часового мультиплексування)
    write_reg(0x03, 0x00);
    
    // Reg 0x04 (Control 3):
    // Біт 7    (DP)   = 0 (PCM режим)
    // Біти 1:0 (SLOW, SD) = initial_filter (Задаємо обраний фільтр)
    write_reg(0x04, (initial_filter & 0x03));
    
    // Reg 0x05 (DSD1):
    // Вимикаємо DSD режим (DSDSEL=0)
    write_reg(0x05, 0x00);
}

void ak5572_start(void) {
    // ==========================================
    // БЛОК 3: ЗАПУСК
    // ==========================================
    // Важливо: Перед цим кроком ADAU1452 вже має генерувати стабільні MCLK, BICK та LRCK!
    
    // Reg 0x00: Вмикаємо живлення лівого і правого каналів (PW1=1, PW2=1). 
    // Старші біти залишаються '1'.
    write_reg(0x00, 0xFF);
    
    // Reg 0x01: Знімаємо Reset (RSTN = 1). Чип починає оцифровку.
    write_reg(0x01, 0x01);
    
    ESP_LOGI(TAG, "AK5572 запущено. Очікування клоків від ADAU1452.");
}

void ak5572_set_filter(uint8_t filter_bits) {
    // Зміна фільтра відбувається у регістрі 0x04 (Control 3), а не 0x02.
    // Біти 7:2 залишаємо нулями (PCM режим).
    write_reg(0x04, (filter_bits & 0x03));
    ESP_LOGI(TAG, "Фільтр AK5572 оновлено: 0x%02X", filter_bits);
}