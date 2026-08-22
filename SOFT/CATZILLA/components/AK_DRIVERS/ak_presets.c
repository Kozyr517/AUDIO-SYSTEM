#include "ak_presets.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "AK_PRESETS";
static QueueHandle_t preset_queue = NULL;

static const ak_preset_t PRESET_LIST[] = {
    { "0: SILK (Sharp)",      AK4493_FILTER_SILK,    AK5572_FILTER_SHARP    },
    { "1: PURRITY (Slow)",    AK4493_FILTER_PURRITY, AK5572_FILTER_SLOW     },
    { "2: DRIVE (SD Sharp)",  AK4493_FILTER_DRIVE,   AK5572_FILTER_SD_SHARP },
    { "3: ATMOS (SD Slow)",   AK4493_FILTER_ATMOS,   AK5572_FILTER_SD_SLOW  },
    { "4: VILVET (SuperSlow)",AK4493_FILTER_VILVET,  AK5572_FILTER_SLOW     },
    { "5: DIRECT (LowDisp)",  AK4493_FILTER_DIRECT,  AK5572_FILTER_SHARP    }
};
#define PRESET_COUNT (sizeof(PRESET_LIST) / sizeof(ak_preset_t))

// ============================================================================
// Фонова задача для обробки пресетів
// ============================================================================
static void ak_presets_task(void *arg) {
    uint8_t preset_idx;
    while (1) {
        if (xQueueReceive(preset_queue, &preset_idx, portMAX_DELAY) == pdTRUE) {
            if (preset_idx >= PRESET_COUNT) continue;

            ESP_LOGI(TAG, ">>> Активація Пресета №%d: %s <<<", preset_idx, PRESET_LIST[preset_idx].name);
            
            // Застосовуємо налаштування фільтрів
            ak5572_set_stereo_mode();
            ak5572_set_filter(PRESET_LIST[preset_idx].adc_filter);
            ak4493_set_filter_all(PRESET_LIST[preset_idx].dac_filter);
        }
    }
}

// ============================================================================
// Головна ініціалізація
// ============================================================================
esp_err_t ak_presets_init(i2c_port_t i2c_num) {
    // Передаємо порт I2C у драйвери і задаємо базові налаштування
    if (ak4493_init(i2c_num) != ESP_OK) return ESP_FAIL;
    if (ak5572_init(i2c_num) != ESP_OK) return ESP_FAIL;

    // Створюємо чергу і запускаємо фонову задачу
    preset_queue = xQueueCreate(5, sizeof(uint8_t));
    if (!preset_queue) return ESP_FAIL;

    xTaskCreatePinnedToCore(ak_presets_task, "ak_presets_task", 4096, NULL, 5, NULL, 1);
    
    return ESP_OK;
}

void ak_presets_apply_by_idx(uint8_t preset_idx) {
    if (preset_queue) xQueueSend(preset_queue, &preset_idx, 0);
}

uint8_t ak_presets_get_count(void) { return PRESET_COUNT; }

const char* ak_presets_get_name(uint8_t preset_idx) {
    return (preset_idx < PRESET_COUNT) ? PRESET_LIST[preset_idx].name : "Unknown";
}