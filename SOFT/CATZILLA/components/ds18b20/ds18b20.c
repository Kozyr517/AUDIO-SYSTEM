#include "ds18b20.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "esp_log.h"

static const char *TAG = "DS18B20";

// Мютекс для критичних секцій (захист таймінгів від FreeRTOS)
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// ================================= НИЗЬКООРІВНЕВИЙ 1-WIRE =================================

static bool onewire_reset(gpio_num_t pin) {
    portENTER_CRITICAL(&mux);
    gpio_set_level(pin, 0);       // Притягуємо лінію до землі
    esp_rom_delay_us(480);        // Імпульс скидання (Reset Pulse)
    
    gpio_set_level(pin, 1);       // Відпускаємо лінію
    esp_rom_delay_us(70);         // Чекаємо відповіді від DS18B20
    
    bool presence = (gpio_get_level(pin) == 0); // Якщо 0 – датчик є на шині
    esp_rom_delay_us(410);        // Чекаємо завершення слота
    
    portEXIT_CRITICAL(&mux);
    return presence;
}

static void onewire_write_bit(gpio_num_t pin, uint8_t bit) {
    portENTER_CRITICAL(&mux);
    gpio_set_level(pin, 0);
    if (bit) {
        esp_rom_delay_us(6);        // Запис '1'
        gpio_set_level(pin, 1);
        esp_rom_delay_us(64);
    } else {
        esp_rom_delay_us(60);       // Запис '0'
        gpio_set_level(pin, 1);
        esp_rom_delay_us(10);
    }
    portEXIT_CRITICAL(&mux);
}

static uint8_t onewire_read_bit(gpio_num_t pin) {
    portENTER_CRITICAL(&mux);
    gpio_set_level(pin, 0);
    esp_rom_delay_us(2);        // Ініціалізація читання
    gpio_set_level(pin, 1);
    esp_rom_delay_us(10);       // Чекаємо стабілізації стану
    
    uint8_t bit = gpio_get_level(pin); // Читаємо значення
    esp_rom_delay_us(50);
    
    portEXIT_CRITICAL(&mux);
    return bit;
}

static void onewire_write_byte(gpio_num_t pin, uint8_t data) {
    int i;
    for (i = 0; i < 8; i++) {
        onewire_write_bit(pin, (data >> i) & 1);
    }
}

static uint8_t onewire_read_byte(gpio_num_t pin) {
    uint8_t byte = 0;
    int i;
    for (i = 0; i < 8; i++) {
        if (onewire_read_bit(pin)) {
            byte |= (1 << i);
        }
    }
    return byte;
}

// ================================= ПУБЛІЧНИЙ ІНТЕРФЕЙС =================================

esp_err_t ds18b20_init(gpio_num_t pin) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT_OUTPUT_OD, // Open-Drain (Відкритий стік)
        .pin_bit_mask = (1ULL << pin),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,     // Внутрішня підтяжка
    };
    esp_err_t err = gpio_config(&io_conf);
    if (err == ESP_OK) {
        gpio_set_level(pin, 1); // Початковий стан – шина вільна
    }
    return err;
}

// Пошук усіх датчиків на шині (Search ROM)
esp_err_t ds18b20_search_devices(gpio_num_t pin, uint8_t rom_addresses[][8], uint8_t max_devices, uint8_t *found_count) {
    uint8_t id_bit_number;
    uint8_t last_zero;
    uint8_t rom_byte_number;
    uint8_t rom_byte_mask;
    uint8_t direction;
    int id_bit;
    int cmp_id_bit;
    int i;
    
    *found_count = 0;
    
    static uint8_t last_discrepancy = 0;
    static uint8_t last_device_flag = 0;
    static uint8_t rom_no[8] = {0};

    if (!onewire_reset(pin)) {
        return ESP_ERR_NOT_FOUND;
    }

    last_discrepancy = 0;
    last_device_flag = 0;
    
    do {
        if (!onewire_reset(pin)) {
            break;
        }
        
        onewire_write_byte(pin, DS18B20_CMD_SEARCH_ROM);
        
        id_bit_number = 1;
        last_zero = 0;
        rom_byte_number = 0;
        rom_byte_mask = 1;
        
        do {
            id_bit = onewire_read_bit(pin);
            cmp_id_bit = onewire_read_bit(pin);
            
            if (id_bit == 1 && cmp_id_bit == 1) {
                break;
            } else {
                if (id_bit != cmp_id_bit) {
                    direction = id_bit;
                } else {
                    if (id_bit_number < last_discrepancy) {
                        direction = ((rom_no[rom_byte_number] & rom_byte_mask) > 0) ? 1 : 0;
                    } else {
                        direction = (id_bit_number == last_discrepancy) ? 1 : 0;
                    }
                    
                    if (direction == 0) {
                        last_zero = id_bit_number;
                    }
                }
                
                if (direction == 1) {
                    rom_no[rom_byte_number] |= rom_byte_mask;
                } else {
                    rom_no[rom_byte_number] &= ~rom_byte_mask;
                }
                
                portENTER_CRITICAL(&mux);
                gpio_set_level(pin, 0);
                if (direction) {
                    esp_rom_delay_us(6);
                    gpio_set_level(pin, 1);
                    esp_rom_delay_us(64);
                } else {
                    esp_rom_delay_us(60);
                    gpio_set_level(pin, 1);
                    esp_rom_delay_us(10);
                }
                portEXIT_CRITICAL(&mux);
                
                id_bit_number++;
                rom_byte_mask <<= 1;
                if (rom_byte_mask == 0) {
                    rom_byte_number++;
                    rom_byte_mask = 1;
                }
            }
        } while (rom_byte_number < 8);
        
        if (!(id_bit_number < 65)) {
            for (i = 0; i < 8; i++) {
                rom_addresses[*found_count][i] = rom_no[i];
            }
            (*found_count)++;
            
            last_discrepancy = last_zero;
            if (last_discrepancy == 0) {
                last_device_flag = 1;
            }
            
            if (*found_count >= max_devices) {
                break;
            }
        }
    } while (!last_device_flag && *found_count < max_devices);

    return (*found_count > 0) ? ESP_OK : ESP_ERR_NOT_FOUND;
}

// Запит температури для конкретного датчика за адресою
esp_err_t ds18b20_request_temperature_addr(gpio_num_t pin, const uint8_t *rom_code) {
    int i;
    if (!onewire_reset(pin)) {
        return ESP_ERR_NOT_FOUND;
    }
    onewire_write_byte(pin, DS18B20_CMD_MATCH_ROM);
    for (i = 0; i < 8; i++) {
        onewire_write_byte(pin, rom_code[i]);
    }
    onewire_write_byte(pin, DS18B20_CMD_CONVERT_T);
    return ESP_OK;
}

// Зчитати температуру для конкретного датчика за адресою
esp_err_t ds18b20_read_temperature_addr(gpio_num_t pin, const uint8_t *rom_code, float *temp) {
    int i;
    if (temp == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!onewire_reset(pin)) {
        return ESP_ERR_NOT_FOUND;
    }
    onewire_write_byte(pin, DS18B20_CMD_MATCH_ROM);
    for (i = 0; i < 8; i++) {
        onewire_write_byte(pin, rom_code[i]);
    }
    onewire_write_byte(pin, DS18B20_CMD_READ_SCRATCH);

    uint8_t lsb = onewire_read_byte(pin);
    uint8_t msb = onewire_read_byte(pin);

    int16_t raw = (msb << 8) | lsb;
    *temp = (float)raw / 16.0f;

    return ESP_OK;
}

// Стандартні функції для одного датчика (Skip ROM)
esp_err_t ds18b20_request_temperature(gpio_num_t pin) {
    if (!onewire_reset(pin)) {
        return ESP_ERR_NOT_FOUND;
    }
    onewire_write_byte(pin, DS18B20_CMD_SKIP_ROM);
    onewire_write_byte(pin, DS18B20_CMD_CONVERT_T);
    return ESP_OK;
}

esp_err_t ds18b20_read_temperature(gpio_num_t pin, float *temp) {
    if (temp == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!onewire_reset(pin)) {
        return ESP_ERR_NOT_FOUND;
    }
    onewire_write_byte(pin, DS18B20_CMD_SKIP_ROM);
    onewire_write_byte(pin, DS18B20_CMD_READ_SCRATCH);
    
    uint8_t lsb = onewire_read_byte(pin);
    uint8_t msb = onewire_read_byte(pin);
    int16_t raw = (msb << 8) | lsb;
    *temp = (float)raw / 16.0f;
    return ESP_OK;
}