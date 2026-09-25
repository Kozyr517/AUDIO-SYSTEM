#ifndef ESP_NOW_RECEIVER_H
#define ESP_NOW_RECEIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Типи команд, які надсилаються з пульта ESP-NOW
typedef enum {
    CMD_SET_MAIN_VOLUME = 0x01, // Загальна гучність (0..255)
    CMD_SET_CH_VOLUME   = 0x02, // Гучність окремого каналу (index: 0..5, value: 0..255)
    CMD_SET_EQ_PRESET   = 0x03, // Пресет еквалайзера (0..5)
    CMD_SET_SURROUND    = 0x04, // Режим Surround/Music (0..1)
    CMD_SET_SYS_MODE    = 0x05, // Режим системи 2.1/5.1 (0..1)
    CMD_SET_DISTANCE    = 0x06  // Затримка/дистанція каналу (index: 0..5, value: 0..255)
} esp_now_cmd_type_t;

// Структура пакета даних ESP-NOW (3 байти, упакована)
typedef struct __attribute__((packed)) {
    uint8_t cmd;    // Команда (esp_now_cmd_type_t)
    uint8_t index;  // Індекс каналу (0..5) або 0 для поодиноких команд
    uint8_t value;  // Значення параметра
} esp_now_msg_t;

/**
 * @brief Ініціалізація Wi-Fi, ESP-NOW приймача та таймера відкладеного збереження.
 * @return esp_err_t ESP_OK у разі успіху.
 */
esp_err_t esp_now_receiver_init(void);

#ifdef __cplusplus
}
#endif

#endif // ESP_NOW_RECEIVER_H