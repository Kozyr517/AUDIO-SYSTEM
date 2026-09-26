#include "ak5572.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stddef.h>

static const char *TAG = "AK5572";

// Локальний хендл саме цього пристрою (АЦП)
static i2c_master_dev_handle_t dev_handle = NULL;

// Функція запису з механізмом повторення (до 3 спроб)
static void write_reg(uint8_t reg, uint8_t data) {
    if (dev_handle == NULL) return;
    
    uint8_t buf[2] = {reg, data};
    esp_err_t err = ESP_FAIL;
    uint8_t retries = 3;
    
    while (retries--) {
        err = i2c_master_transmit(dev_handle, buf, 2, -1);
        
        if (err == ESP_OK) {
            return; // Успішний запис, виходимо
        }
        
        vTaskDelay(pdMS_TO_TICKS(5)); // Чекаємо 5 мс перед наступною спробою
    }
    
    ESP_LOGE(TAG, "Помилка I2C (Регістр 0x%02X) після 3 спроб: %s", reg, esp_err_to_name(err));
}

void ak5572_init(i2c_master_bus_handle_t bus_handle, uint8_t i2c_addr, uint8_t initial_filter) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = i2c_addr,
        .scl_speed_hz = 400000,
    };
    i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);

    // Згідно з даташитом AKM, конфігурацію треба робити при RSTN = 0 (в стані скидання)
    
    // Reg 0x00 (Power Management 1): Вмикаємо живлення лівого і правого каналів (L+R = 0x03)
    write_reg(0x00, 0x03); 
    
    // Reg 0x01 (Power Management 2): Утримуємо чип в стані Reset (RSTN = 0) на час налаштування
    write_reg(0x01, 0x00);
    
    // Reg 0x02 (Control 1): 
    // 0x80 (HPFE = 1, зріз DC) + 0x60 (FMT = 11, 32-bit I2S) = 0xE0. 
    // Додаємо до 0xE0 обраний фільтр.
    write_reg(0x02, 0xE0 | (initial_filter & 0x03));
    
    // Reg 0x03 (Control 2): 0x00 = Режим Slave (ADAU генерує I2S), автоматичний вибір частоти
    write_reg(0x03, 0x00);
    
    // Reg 0x04 (Control 3): 0x00 = Автоматичне визначення швидкості FS за MCLK
    write_reg(0x04, 0x00);
}

void ak5572_start(void) {
    // Reg 0x01: Знімаємо Reset (RSTN = 1), чип починає оцифровку і видачу 32-бітних даних
    write_reg(0x01, 0x01);
}

void ak5572_set_filter(uint8_t filter_bits) {
    // При зміні фільтра на льоту, обов'язково зберігаємо налаштування HPF та 32-bit I2S (0xE0)
    write_reg(0x02, 0xE0 | (filter_bits & 0x03));
}