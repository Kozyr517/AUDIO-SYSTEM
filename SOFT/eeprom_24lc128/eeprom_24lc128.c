#include "eeprom_24lc128.h"
#include "i2c_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stddef.h> // Необхідно для offsetof

static const char *TAG = "EEPROM";
static i2c_master_dev_handle_t eeprom_dev_handle = NULL;

#define I2C_TIMEOUT_MS 100 // Таймаут для I2C операцій

// Глобальні змінні для роботи в RAM та перевірки змін
app_settings_t g_settings = {0};
app_settings_t g_saved_settings = {0};
volatile bool is_settings_dirty = false;

esp_err_t eeprom_init(void) {
    if (i2c_bus_handle == NULL) {
        ESP_LOGE(TAG, "I2C bus not initialized!");
        return ESP_ERR_INVALID_STATE;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = EEPROM_I2C_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    esp_err_t ret = i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &eeprom_dev_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "EEPROM 24LC128 registered at 0x%02X", EEPROM_I2C_ADDR);
    } else {
        ESP_LOGE(TAG, "Failed to add EEPROM to I2C bus");
    }
    return ret;
}

uint8_t eeprom_calculate_crc8(const uint8_t *data, size_t length) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07; // Dallas/Maxim поліном
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

esp_err_t eeprom_read_bytes(uint16_t mem_addr, uint8_t *data, size_t len) {
    if (eeprom_dev_handle == NULL || data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t addr_buf[2] = {
        (uint8_t)(mem_addr >> 8),   // High byte
        (uint8_t)(mem_addr & 0xFF)  // Low byte
    };

    return i2c_master_transmit_receive(
        eeprom_dev_handle, 
        addr_buf, 
        sizeof(addr_buf), 
        data, 
        len, 
        pdMS_TO_TICKS(I2C_TIMEOUT_MS)
    );
}

esp_err_t eeprom_write_bytes(uint16_t mem_addr, const uint8_t *data, size_t len) {
    if (eeprom_dev_handle == NULL || data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t bytes_written = 0;

    while (bytes_written < len) {
        uint16_t current_addr = mem_addr + bytes_written;
        uint16_t page_offset = current_addr % EEPROM_PAGE_SIZE;
        size_t chunk_size = EEPROM_PAGE_SIZE - page_offset;

        if (chunk_size > (len - bytes_written)) {
            chunk_size = len - bytes_written;
        }

        uint8_t write_buf[2 + EEPROM_PAGE_SIZE];
        write_buf[0] = (uint8_t)(current_addr >> 8);
        write_buf[1] = (uint8_t)(current_addr & 0xFF);
        memcpy(&write_buf[2], &data[bytes_written], chunk_size);

        esp_err_t ret = i2c_master_transmit(
            eeprom_dev_handle, 
            write_buf, 
            2 + chunk_size, 
            pdMS_TO_TICKS(I2C_TIMEOUT_MS)
        );
        
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Write error at addr 0x%04X: %s", current_addr, esp_err_to_name(ret));
            return ret;
        }

        bytes_written += chunk_size;
        vTaskDelay(pdMS_TO_TICKS(10)); // Час на внутрішній запис EEPROM (tWC)
    }

    return ESP_OK;
}

esp_err_t eeprom_load_settings(app_settings_t *settings) {
    if (settings == NULL) return ESP_ERR_INVALID_ARG;

    esp_err_t ret = eeprom_read_bytes(0x0000, (uint8_t *)settings, sizeof(app_settings_t));
    
    // 1. Перевірка результату читання та Magic Byte
    bool valid = (ret == ESP_OK) && (settings->magic == SETTINGS_MAGIC);

    // 2. Валідація контрольної суми CRC8
    if (valid) {
        uint8_t calc_crc = eeprom_calculate_crc8((const uint8_t *)settings, offsetof(app_settings_t, crc8));
        if (calc_crc != settings->crc8) {
            ESP_LOGE(TAG, "CRC error! Read: 0x%02X, Calc: 0x%02X", settings->crc8, calc_crc);
            valid = false;
        }
    }

    // 3. Завантаження дефолтних налаштувань у разі помилки/порожньої пам'яті
    if (valid) {
        ESP_LOGI(TAG, "Settings loaded and verified successfully (CRC8: 0x%02X)", settings->crc8);
        memcpy(&g_saved_settings, settings, sizeof(app_settings_t));
    } else {
        ESP_LOGW(TAG, "EEPROM empty or CRC mismatch. Writing default profile...");

        memset(settings, 0, sizeof(app_settings_t));
        settings->magic = SETTINGS_MAGIC;
        
        settings->ak5572_preset = 0;
        settings->ak4493_filter = 0;

        settings->adau_main_volume = 200; // ~ -20 dB
        
        memset(settings->adau_vol_ch, 255, sizeof(settings->adau_vol_ch)); // Всі канали 0 dB
        memset(settings->adau_distances, 0, sizeof(settings->adau_distances)); // Дистанції 0
        
        settings->adau_eq_preset = 0;     // Studio Flat
        settings->adau_surround_mode = 0; // Music Mode (Stereo)
        settings->sys_mode = 1;           // 5.1 System Mode

        ret = eeprom_save_settings(settings);
        if (ret == ESP_OK) {
            memcpy(&g_saved_settings, settings, sizeof(app_settings_t));
        }
    }
    
    return ret;
}

esp_err_t eeprom_save_settings(const app_settings_t *settings) {
    if (settings == NULL) return ESP_ERR_INVALID_ARG;

    app_settings_t temp_settings = *settings;
    temp_settings.magic = SETTINGS_MAGIC;
    temp_settings.crc8 = eeprom_calculate_crc8((const uint8_t *)&temp_settings, offsetof(app_settings_t, crc8));

    ESP_LOGI(TAG, "Saving settings to EEPROM... (CRC8: 0x%02X)", temp_settings.crc8);
    return eeprom_write_bytes(0x0000, (const uint8_t *)&temp_settings, sizeof(app_settings_t));
}

bool settings_commit_check(void) {
    // Знімаємо локальну копію актуальних налаштувань для потокобезпечності
    app_settings_t current_snapshot = g_settings;

    if (memcmp(&current_snapshot, &g_saved_settings, sizeof(app_settings_t)) != 0) {
        ESP_LOGI(TAG, "Detected changes in RAM. Committing to EEPROM...");
        
        esp_err_t err = eeprom_save_settings(&current_snapshot);
        
        if (err == ESP_OK) {
            // Оновлюємо збережений еталон значеннями, які реально відправили в EEPROM
            current_snapshot.crc8 = eeprom_calculate_crc8((const uint8_t *)&current_snapshot, offsetof(app_settings_t, crc8));
            memcpy(&g_saved_settings, &current_snapshot, sizeof(app_settings_t));
            is_settings_dirty = false;
            ESP_LOGI(TAG, "EEPROM commit successful.");
            return true;
        } else {
            ESP_LOGE(TAG, "EEPROM commit failed!");
        }
    } else {
        ESP_LOGI(TAG, "Settings unchanged. Commit bypassed.");
        is_settings_dirty = false;
    }
    return false;
}