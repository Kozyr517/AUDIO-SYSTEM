#include "esp_now_receiver.h"
#include "eeprom_24lc128.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_idf_version.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

static const char *TAG = "ESP_NOW_RX";

// Таймер відкладеного запису в EEPROM (60 секунд)
static TimerHandle_t save_timer = NULL;
#define SAVE_DELAY_MS 60000

// Колбек таймера FreeRTOS: викликається після 60 с тиші в ефірі
static void save_timer_callback(TimerHandle_t xTimer) {
    ESP_LOGI(TAG, "60s timer expired. Triggering EEPROM commit check...");
    settings_commit_check();
}

// Перезапуск таймера на 60 секунд
static void reset_save_timer(void) {
    if (save_timer != NULL) {
        xTimerReset(save_timer, 0);
    }
}

// Обробник отриманих пакетів ESP-NOW (сумісний із версіями ESP-IDF v4.x та v5.x)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
static void esp_now_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
#else
static void esp_now_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int len)
#endif
{
    if (data == NULL || len != sizeof(esp_now_msg_t)) {
        ESP_LOGW(TAG, "Invalid packet length: %d (expected %d)", len, (int)sizeof(esp_now_msg_t));
        return;
    }

    const esp_now_msg_t *msg = (const esp_now_msg_t *)data;
    bool setting_changed = false;

    switch (msg->cmd) {
        case CMD_SET_MAIN_VOLUME:
            if (g_settings.adau_main_volume != msg->value) {
                g_settings.adau_main_volume = msg->value;
                setting_changed = true;
                ESP_LOGI(TAG, "Rx Main Vol: %d", msg->value);
            }
            break;

        case CMD_SET_CH_VOLUME:
            if (msg->index < 6) {
                if (g_settings.adau_vol_ch[msg->index] != msg->value) {
                    g_settings.adau_vol_ch[msg->index] = msg->value;
                    setting_changed = true;
                    ESP_LOGI(TAG, "Rx Ch[%d] Vol: %d", msg->index, msg->value);
                }
            } else {
                ESP_LOGW(TAG, "Invalid channel index: %d", msg->index);
            }
            break;

        case CMD_SET_EQ_PRESET:
            if (msg->value <= 5) {
                if (g_settings.adau_eq_preset != msg->value) {
                    g_settings.adau_eq_preset = msg->value;
                    setting_changed = true;
                    ESP_LOGI(TAG, "Rx EQ Preset: %d", msg->value);
                }
            } else {
                ESP_LOGW(TAG, "Invalid EQ Preset: %d", msg->value);
            }
            break;

        case CMD_SET_SURROUND:
            if (msg->value <= 1) {
                if (g_settings.adau_surround_mode != msg->value) {
                    g_settings.adau_surround_mode = msg->value;
                    setting_changed = true;
                    ESP_LOGI(TAG, "Rx Surround Mode: %d", msg->value);
                }
            } else {
                ESP_LOGW(TAG, "Invalid Surround Mode: %d", msg->value);
            }
            break;

        case CMD_SET_SYS_MODE:
            if (msg->value <= 1) {
                if (g_settings.sys_mode != msg->value) {
                    g_settings.sys_mode = msg->value;
                    setting_changed = true;
                    ESP_LOGI(TAG, "Rx Sys Mode: %d", msg->value);
                }
            } else {
                ESP_LOGW(TAG, "Invalid Sys Mode: %d", msg->value);
            }
            break;

        case CMD_SET_DISTANCE:
            if (msg->index < 6) {
                if (g_settings.adau_distances[msg->index] != msg->value) {
                    g_settings.adau_distances[msg->index] = msg->value;
                    setting_changed = true;
                    ESP_LOGI(TAG, "Rx Distance[%d]: %d", msg->index, msg->value);
                }
            } else {
                ESP_LOGW(TAG, "Invalid Distance index: %d", msg->index);
            }
            break;

        default:
            ESP_LOGW(TAG, "Unknown command code: 0x%02X", msg->cmd);
            break;
    }

    if (setting_changed) {
        is_settings_dirty = true;
        reset_save_timer();
    }
}

esp_err_t esp_now_receiver_init(void) {
    // 1. Ініціалізація NVS (необхідна для роботи Wi-Fi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS Init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 2. Безпечна ініціалізація мережевого стеку та Event Loop
    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_event_loop_create_default failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 3. Ініціалізація Wi-Fi у режимі Station
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 4. Ініціалізація ESP-NOW та реєстрація callback
    ret = esp_now_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_now_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_ERROR_CHECK(esp_now_register_recv_cb(esp_now_recv_cb));

    // 5. Створення FreeRTOS таймера відкладеного збереження (60 секунд)
    save_timer = xTimerCreate("SaveTimer", pdMS_TO_TICKS(SAVE_DELAY_MS), pdFALSE, NULL, save_timer_callback);
    if (save_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create save timer!");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "ESP-NOW Receiver & 60s Lazy-Save Timer initialized successfully");
    return ESP_OK;
}