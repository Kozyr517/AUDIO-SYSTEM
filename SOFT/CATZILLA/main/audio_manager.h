#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "app_state.h"
#include "adau1452.h"

// Фізичні I2C адреси мікросхем на платі
#define AK4493_DAC_ADDR_1    0x10 // ЦАП 1
#define AK4493_DAC_ADDR_2    0x11 // ЦАП 2
#define AK4493_DAC_ADDR_3    0x12 // ЦАП 3
#define AK5572_ADC_ADDR      0x13 // АЦП
#define ADAU1452_DSP_ADDR    0x38 // DSP

// 1. Прапорці наявності сигналу з 4 блоків ReadBack DSP
typedef struct {
    bool line_l;
    bool line_r;
    bool ble;
    bool toslink;
} scanner_flags_t;

// 2. Режим псевдостерео аналогового входу (Nx2-1_5)
typedef enum {
    LINE_MODE_STEREO     = 0,
    LINE_MODE_MONO_L_ALL = 1,
    LINE_MODE_MONO_R_ALL = 2
} line_input_mode_t;

// 3. Селектор джерела входу (Nx2-1_3)
typedef enum {
    INPUT_SRC_ANALOG  = 0,
    INPUT_SRC_BLE     = 1,
    INPUT_SRC_OPTICAL = 2
} audio_input_src_t;

// 5. Конфігурація акустики (Nx2-1_4 + Mute1)
typedef enum {
    SPEAKER_MODE_2_1 = 0,
    SPEAKER_MODE_5_1 = 1
} speaker_mode_t;

// 6-7. Канали акустики (порядок узгоджено з menu.c -> myspace_speaker_names)
typedef enum {
    CH_FRONT_L = 0,
    CH_CENTER  = 1,
    CH_BASS    = 2,
    CH_FRONT_R = 3,
    CH_REAR_L  = 4,
    CH_REAR_R  = 5,
    CH_COUNT   = 6
} audio_channel_t;

// Поточний стан параметрів DSP у RAM
typedef struct {
    line_input_mode_t line_mode;
    audio_input_src_t input_source;
    uint8_t           eq_preset;
    speaker_mode_t    speaker_mode;
    float             delays_ms[CH_COUNT];
    uint8_t           ch_vol_pct[CH_COUNT];
    uint8_t           surround_room;
    uint8_t           surround_mode;
} dsp_runtime_state_t;

// Ініціалізація аудіотракту, застосування налаштувань та фільтрів AKM
void audio_manager_init_all(void);
void audio_manager_apply_all_settings(void);
void audio_update_filters(uint8_t ui_filter_index);

// Керування блоками ADAU1452
esp_err_t audio_manager_read_scanner(scanner_flags_t *out_flags);               // Сканер сигналу (4 ReadBack -> готові bool прапорці для autosearch.c)
esp_err_t audio_manager_set_line_mode(line_input_mode_t mode);                  // Псевдостерео Line (Nx2-1_5) — на майбутнє
esp_err_t audio_manager_set_input_source(audio_input_src_t src);                // Селектор входів (Nx2-1_3: Analog / BLE / Op)tical
esp_err_t audio_manager_set_eq_preset(eq_preset_id_t preset);                   // 15-смуговий EQ (Mid Filter1)
esp_err_t audio_manager_set_speaker_mode(speaker_mode_t mode);                  // Режим 2.1 / 5.1 (Nx2-1_4 + автоматичний Mute1 для відсікання зайвих каналів у 2.1)
esp_err_t audio_manager_set_channel_delay(audio_channel_t ch, float delay_ms);  // Затримки каналів (меню My Space, автоматичний ліміт мс по кожному каналу)
esp_err_t audio_manager_set_channel_volume(audio_channel_t ch, uint8_t pct);    // Поканальні гучності 0..100% (меню My Space)
esp_err_t audio_manager_set_master_volume(uint8_t pct);                         // Master-гучність 0..100% (Single 4_2) + синхронізація з глобальним master_volume
esp_err_t audio_manager_set_master_mute(bool mute);                             // Загальний Mute (Mute2) + синхронізація з глобальним is_muted
esp_err_t audio_manager_set_surround_mode(surround_mode_t mode);                // ADI Surround: режим (Music/Surround) і простір (Small/Large)
esp_err_t audio_manager_set_surround_room(surround_room_t room);                // ADI Surround: режим (Music/Surround) і простір (Small/Large)
const dsp_runtime_state_t* audio_manager_get_dsp_state(void);                   // Отримання поточного стану параметрів DSP

#endif // AUDIO_MANAGER_H