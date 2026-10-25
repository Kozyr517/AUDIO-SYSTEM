#include "ak4493.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stddef.h>

static const char *TAG = "AK4493";

// Локальні хендли трьох ЦАПів
static i2c_master_dev_handle_t dac_handles[3] = {NULL, NULL, NULL};

// Запис у всі 3 ЦАПи з механізмом повторення
static void write_reg_all(uint8_t reg, uint8_t data) {
    uint8_t buf[2] = {reg, data};
    
    for (int i = 0; i < 3; i++) {
        if (dac_handles[i] != NULL) {
            esp_err_t err = ESP_FAIL;
            uint8_t retries = 3;
            bool success = false;
            
            while (retries--) {
                err = i2c_master_transmit(dac_handles[i], buf, 2, 100);
                
                if (err == ESP_OK) {
                    success = true;
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(5)); 
            }
            
            if (!success) {
                ESP_LOGE(TAG, "ЦАП %d (0x%02X) НЕ ВІДПОВІДАЄ (Рег 0x%02X): %s", 
                         i + 1, 0x10 + i, reg, esp_err_to_name(err));
            }
        }
    }
}

void ak4493_init_all(i2c_master_bus_handle_t bus_handle, const uint8_t *i2c_addrs, uint8_t initial_filter) {
    for (int i = 0; i < 3; i++) {
        // Додаємо пристрій лише якщо він ще не зареєстрований (дозволяє повторний виклик init)
        if (dac_handles[i] == NULL) {
            i2c_device_config_t dev_cfg = {
                .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                .device_address = i2c_addrs[i],
                .scl_speed_hz = 400000,
            };
            i2c_master_bus_add_device(bus_handle, &dev_cfg, &dac_handles[i]);
        }
    }

    // 1. Reg 0x00 (Control 1): 
    // ACKS=0 (Ручний режим 192кГц — надійніше за авто), DIF=111 (32-bit I2S), RSTN=0 (Утримуємо Reset)
    // Байт: 0000 1110 = 0x0E
    write_reg_all(0x00, 0x0E);
    vTaskDelay(pdMS_TO_TICKS(5));
    
    // 2. Встановлення цифрового фільтра + жорстко Quad Speed 192kHz (регістри 0x01, 0x02, 0x05)
    ak4493_set_filter_all(initial_filter);
    
    // 3. Reg 0x03 & 0x04: Гучність L/R = 0xFF (0 dB, максимум)
    write_reg_all(0x03, 0xFF);
    write_reg_all(0x04, 0xFF);
    
    // 4. Reg 0x06 (DSD1): Примусово нуль
    write_reg_all(0x06, 0x00);

    // 5. Reg 0x07 (Control 5): GC=000 (2.8 Vpp), SYNCE=1 (Синхронізація 3-х ЦАПів увімкнена за дефолтом)
    write_reg_all(0x07, 0x01);

    // 6. Reg 0x08 (Sound Control): Дефолтний режим
    write_reg_all(0x08, 0x00);

    vTaskDelay(pdMS_TO_TICKS(10));
}

void ak4493_start_all(void) {
    // Reg 0x00: Спочатку гарантовано скидаємо RSTN=0 на 5 мс, потім піднімаємо RSTN=1 (0x0F).
    // Це перезапускає внутрішню синхронізацію ЦАПів під поточні клоки ADAU1452!
    write_reg_all(0x00, 0x0E);
    vTaskDelay(pdMS_TO_TICKS(5));
    write_reg_all(0x00, 0x0F);
    ESP_LOGI(TAG, "Усі 3 ЦАПи AK4493 успішно запущені (192kHz 32-bit I2S)");
}

void ak4493_set_filter_all(uint8_t filter_bits) {
    uint8_t sd    = filter_bits & AK_SD_BIT;               // Біт 5 (0x20)
    uint8_t slow  = (filter_bits & AK_SLOW_BIT) ? 0x01 : 0x00;  // Біт 0 (0x01)
    uint8_t sslow = (filter_bits & AK_SSLOW_BIT) ? 0x01 : 0x00; // Біт 0 (0x01)

    // Reg 0x01 (Control 2):
    // DFS[1:0] = 10 (0x10 -> Quad Speed 192kHz)
    // DEM[1:0] = 01 (0x02 -> De-emphasis OFF)
    // SMUTE    = 0  (Mute OFF)
    // Базове значення: 0x12 | sd
    write_reg_all(0x01, 0x12 | sd);
    
    // Reg 0x02 (Control 3): DP=0 (PCM), MONO=0 (Stereo) + біт SLOW
    write_reg_all(0x02, slow);
    
    // Reg 0x05 (Control 4): INVL=0, INVR=0, DFS2=0 + біт SSLOW
    write_reg_all(0x05, sslow);
}