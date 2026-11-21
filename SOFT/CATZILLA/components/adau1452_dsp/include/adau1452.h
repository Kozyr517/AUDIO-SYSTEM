#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "adau1452_regs.h"

// Пресети 15-смугового EQ
typedef enum {
    EQ_PRESET_FLAT = 0,
    EQ_PRESET_ROCK,
    EQ_PRESET_POP,
    EQ_PRESET_JAZZ,
    EQ_PRESET_CUSTOM,
    EQ_PRESET_MAX
} eq_preset_id_t;

// ADI Surround: розмір приміщення (меню My Space)
typedef enum {
    SURROUND_ROOM_SMALL = 0,
    SURROUND_ROOM_LARGE = 1
} surround_room_t;

// ADI Surround: режим звучання (основне меню)
typedef enum {
    SURROUND_MODE_MUSIC    = 0,
    SURROUND_MODE_SURROUND = 1
} surround_mode_t;

// Реєстрація ADAU1452 на шині i2c_bus_handle (тільки I2C-дескриптор ESP32, без запису в ядро DSP)
esp_err_t adau1452_init(i2c_master_bus_handle_t bus_handle, uint8_t dev_addr);

// Базові операції з 32-бітними параметрами DM0/DM1
esp_err_t adau1452_write_u32(uint16_t reg_addr, uint32_t value);
esp_err_t adau1452_read_u32(uint16_t reg_addr, uint32_t *out_value);

// Запис через SafeLoad (1..5 слів за транзакцію без клацань)
esp_err_t adau1452_safeload_write(uint16_t target_addr, const uint32_t *data, uint8_t word_count);

// Перемикання Slew Stereo Mux (ставить 1.0 на активний вхід і 0.0 на решту)
esp_err_t adau1452_set_slew_mux(uint16_t base_addr, uint8_t num_inputs, uint8_t active_idx);

// Завантаження пресетів із adau1452_presets.c
esp_err_t adau1452_apply_eq_preset(eq_preset_id_t preset);
esp_err_t adau1452_apply_surround(surround_room_t room, surround_mode_t mode);