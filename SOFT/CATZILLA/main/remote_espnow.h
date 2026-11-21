/*
 * ============================================================================
 * ЗАКОМЕНТОВАНИЙ МОДУЛЬ ПРИЙМАЧА ПУЛЬТА ESP-NOW (НЕ ПІДНІМАЄ WI-FI ЗАРАЗ)
 *
 * Як увімкнути пізніше, коли пульт буде готовий:
 *   1. Розкоментуйте вміст цього файлу (прибрати обгортку / * ... * /).
 *   2. Розкоментуйте код у main/remote_espnow.c.
 *   3. У main/CMakeLists.txt додайте "remote_espnow.c" до SRCS та
 *      esp_wifi nvs_flash esp_netif esp_event до REQUIRES.
 *   4. У main/main.c розкоментуйте #include "remote_espnow.h" та remote_espnow_init();
 * ============================================================================
 */

/*
#ifndef REMOTE_ESPNOW_H
#define REMOTE_ESPNOW_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

// Магічне слово протоколу CATZILLA Remote (відсіює чужі пакети з ефіру)
#define CATZILLA_REMOTE_MAGIC     0xCA721E01UL
#define CATZILLA_ESPNOW_CHANNEL   1

// Налаштування прив'язки до конкретного пульта:
//   0 = перевірка лише по 128-бітному секретному ключу (auth_token + CRC16)
//   1 = додаткова жорстка прив'язка до MAC-адреси пульта (CATZILLA_REMOTE_MAC)
#define CATZILLA_USE_MAC_BINDING  0

// 128-бітний секретний шифр (має бути ІДЕНТИЧНИМ у прошивці пульта і підсилювача!)
// Можна змінити на будь-які 16 власних байтів перед прошивкою пари.
static const uint8_t CATZILLA_REMOTE_SECRET_KEY[16] = {
    0xCA, 0x72, 0x11, 0x1A, 0x43, 0x41, 0x54, 0x5A,
    0x9E, 0x3B, 0x7F, 0x20, 0x55, 0xA1, 0x09, 0xD4
};

#if CATZILLA_USE_MAC_BINDING
// Сюди вписати реальну MAC-адресу ESP32 вашого пульта (Wi-Fi STA MAC):
static const uint8_t CATZILLA_REMOTE_MAC[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
#endif

// Команди пульта (ТІЛЬКИ гучність, пресети та режими; простір My Space і голки Phono — лише вручну)
typedef enum {
    RCMD_VOL_UP = 1,        // Збільшити гучність (param = крок, напр. 1 або 2)
    RCMD_VOL_DOWN,          // Зменшити гучність (param = крок)
    RCMD_VOL_SET,           // Встановити точну гучність (param = 0..100%)
    RCMD_MUTE_TOGGLE,       // Перемкнути MUTE (увімк/вимк)
    RCMD_MUTE_SET,          // Точний стан MUTE: param = 1 (MUTE ON), 0 (MUTE OFF)
    RCMD_EQ_NEXT,           // Наступний пресет еквалайзера (циклічно 0..5)
    RCMD_EQ_SET,            // Конкретний пресет еквалайзера (param = 0..5: POP..B&T)
    RCMD_FILTER_NEXT,       // Наступний пресет цифрового фільтра AKM (циклічно 0..5)
    RCMD_FILTER_SET,        // Конкретний фільтр AKM (param = 0..5: SILK..DIRECT)
    RCMD_SPATIAL_TOGGLE,    // Перемкнути CINEMA <-> MUSIC (у 2.1 заблоковано в MUSIC)
    RCMD_SPATIAL_SET,       // Конкретний режим: param = 0 (CINEMA) або 1 (MUSIC)
    RCMD_SYS_MODE_TOGGLE,   // Перемкнути SYS 2.1 <-> SYS 5.1
    RCMD_SYS_MODE_SET,      // Конкретний режим: param = 0 (2.1) або 1 (5.1)
    RCMD_FORCE_SCAN,        // Запустити 60-сек автопошук входу (аналог короткого кліку OK)
    RCMD_POWER_TOGGLE,      // Увімкнути (якщо в Standby) або Вимкнути (якщо увімкнено)
    RCMD_POWER_ON,          // Примусово увімкнути зі Standby
    RCMD_POWER_OFF          // Примусово вимкнути (перехід у Standby / знеструмлення периферії)
} remote_cmd_id_t;

// Компактний пакет ESP-NOW (рівно 16 байт)
typedef struct __attribute__((packed)) {
    uint32_t magic;       // CATZILLA_REMOTE_MAGIC (0xCA721E01)
    uint32_t seq_id;      // Наскрізний лічильник натискань (захист від дублів при повторах)
    uint8_t  cmd;         // Код команди (remote_cmd_id_t)
    uint8_t  param;       // Параметр команди (0..100 або індекс)
    uint32_t auth_token;  // Крипто-підпис (хеш від SECRET_KEY + seq_id + cmd + param)
    uint16_t crc16;       // Контрольна сума CRC-16-CCITT перших 14 байт
} remote_packet_t;

// Розрахунок підпису та CRC16 (однакова функція має бути і на стороні пульта!)
uint16_t remote_calc_crc16(const uint8_t *data, size_t len);
uint32_t remote_calc_auth_token(uint32_t seq_id, uint8_t cmd, uint8_t param, const uint8_t *src_mac);

// Ініціалізація радіоприймача ESP-NOW (STA без підключення до Wi-Fi мережі)
esp_err_t remote_espnow_init(void);

// Перевірка та скидання прапорця пробудження з пульта (використовується в режимі Standby у main.c)
bool remote_espnow_consume_wake_request(void);

#endif // REMOTE_ESPNOW_H
*/
