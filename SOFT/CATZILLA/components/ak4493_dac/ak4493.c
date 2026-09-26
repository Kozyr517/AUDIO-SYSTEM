#include "ak4493.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stddef.h>

static const char *TAG = "AK4493";

// Локальні хендли трьох ЦАПів
static i2c_master_dev_handle_t dac_handles[3] = {NULL, NULL, NULL};

// Запис у всі 3 ЦАПи з механізмом повторення для кожного
static void write_reg_all(uint8_t reg, uint8_t data) {
    uint8_t buf[2] = {reg, data};
    
    for (int i = 0; i < 3; i++) {
        if (dac_handles[i] != NULL) {
            esp_err_t err = ESP_FAIL;
            uint8_t retries = 3;
            bool success = false;
            
            while (retries--) {
                err = i2c_master_transmit(dac_handles[i], buf, 2, -1);
                
                if (err == ESP_OK) {
                    success = true;
                    break; // Успішний запис, переходимо до наступного ЦАПа
                }
                
                vTaskDelay(pdMS_TO_TICKS(5)); 
            }
            
            if (!success) {
                ESP_LOGE(TAG, "ЦАП %d НЕ ВІДПОВІДАЄ після 3 спроб! Помилка (Регістр 0x%02X): %s", i + 1, reg, esp_err_to_name(err));
            }
        }
    }
}

void ak4493_init_all(i2c_master_bus_handle_t bus_handle, const uint8_t *i2c_addrs, uint8_t initial_filter) {
    for (int i = 0; i < 3; i++) {
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = i2c_addrs[i],
            .scl_speed_hz = 400000,
        };
        i2c_master_bus_add_device(bus_handle, &dev_cfg, &dac_handles[i]);
    }

    // Reg 0x00: 0x9C (ACKS=1 Auto Clock, DIF=111 32-bit I2S, RSTN=0 Reset)
    write_reg_all(0x00, 0x9C);
    
    // Reg 0x01: Вибір цифрового фільтра звуку
    write_reg_all(0x01, initial_filter);
    
    // Reg 0x02: Дефолт (Stereo Mode)
    write_reg_all(0x02, 0x00);
    
    // Reg 0x03 & 0x04: Гучність L/R = 0xFF (Максимум, 0 dB). Керуємо гучністю лише через DSP!
    write_reg_all(0x03, 0xFF);
    write_reg_all(0x04, 0xFF);
    
    // Reg 0x07: Режим нормальної роботи модуляторів
    write_reg_all(0x07, 0x04);
}

void ak4493_start_all(void) {
    // Reg 0x00: 0x9D (Знімаємо Reset, RSTN=1, починаємо відтворення)
    write_reg_all(0x00, 0x9D);
}

void ak4493_set_filter_all(uint8_t filter_bits) {
    // Оновлення фільтра "на льоту" з меню
    write_reg_all(0x01, filter_bits);
}