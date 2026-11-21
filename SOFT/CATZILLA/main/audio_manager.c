#include "audio_manager.h"
#include <math.h>
#include "eeprom_24lc128.h" 
#include "i2c_bus.h"        
#include "ak5572.h"         
#include "ak4493.h"         
#include "adau1452.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "AUDIO_MGR";

#define DSP_SAMPLE_RATE_KHZ 192.0f

// Масив адрес трьох ЦАПів
static const uint8_t dac_addrs[3] = {AK4493_DAC_ADDR_1, AK4493_DAC_ADDR_2, AK4493_DAC_ADDR_3};

// Карти фільтрів (Ідеальне рішення для відв'язки UI від Hardware)
static const uint8_t ak5572_filter_map[6] = {
    ADC_FILTER_SHARP_ROLLOFF,        // [0] SILK
    ADC_FILTER_SLOW_ROLLOFF,         // [1] PURRITY
    ADC_FILTER_SHORT_DELAY_SHARP,    // [2] DRIVE
    ADC_FILTER_SHORT_DELAY_SLOW,     // [3] ATMOS
    ADC_FILTER_SLOW_ROLLOFF,         // [4] VILVET  
    ADC_FILTER_SHARP_ROLLOFF         // [5] DIRECT  
};

static const uint8_t ak4493_filter_map[6] = {
    DAC_FILTER_LOW_DISPERSION_SHORT_DELAY, // [0] SILK
    DAC_FILTER_SHORT_DELAY_SHARP,          // [1] PURRITY
    DAC_FILTER_SHARP_ROLLOFF,              // [2] DRIVE
    DAC_FILTER_SLOW_ROLLOFF,               // [3] ATMOS
    DAC_FILTER_SUPER_SLOW,                 // [4] VILVET
    DAC_FILTER_SHORT_DELAY_SLOW            // [5] DIRECT
};

// Таблиця адрес затримок по каналах (ADAU1452)
static const uint16_t k_delay_regs[CH_COUNT] = {
    [CH_BASS]    = REG_DELAY_BASS,
    [CH_FRONT_L] = REG_DELAY_FRONT_L,
    [CH_FRONT_R] = REG_DELAY_FRONT_R,
    [CH_CENTER]  = REG_DELAY_CENTER,
    [CH_REAR_L]  = REG_DELAY_REAR_L,
    [CH_REAR_R]  = REG_DELAY_REAR_R
};

// Ліміти максимальних затримок по каналах (мс)
static const float k_max_delay_ms[CH_COUNT] = {
    [CH_BASS]    = 15.0f, // Delay4:   макс 15 мс (2880 семплів)
    [CH_FRONT_L] = 10.0f, // Delay4_2: макс 10 мс (1920 семплів)
    [CH_FRONT_R] = 10.0f, // Delay4_3: макс 10 мс (1920 семплів)
    [CH_CENTER]  =  5.0f, // Delay4_6: макс 5 мс  (960 семплів)
    [CH_REAR_L]  = 30.0f, // Delay4_4: макс 30 мс (5760 семплів)
    [CH_REAR_R]  = 30.0f  // Delay4_5: макс 30 мс (5760 семплів)
};

// Таблиця адрес індивідуальних гучностей по каналах (ADAU1452)
static const uint16_t k_vol_regs[CH_COUNT] = {
    [CH_BASS]    = REG_VOL_BASS,
    [CH_FRONT_L] = REG_VOL_FRONT_L,
    [CH_FRONT_R] = REG_VOL_FRONT_R,
    [CH_CENTER]  = REG_VOL_CENTER,
    [CH_REAR_L]  = REG_VOL_REAR_L,
    [CH_REAR_R]  = REG_VOL_REAR_R
};

// Поточний стан параметрів DSP у RAM
static dsp_runtime_state_t s_dsp = {
    .line_mode     = LINE_MODE_STEREO,
    .input_source  = INPUT_SRC_ANALOG,
    .eq_preset     = EQ_PRESET_FLAT,
    .speaker_mode  = SPEAKER_MODE_5_1,
    .delays_ms     = { 0.0f, 0.0f, 0.0f, 0.0f, 15.0f, 15.0f },
    .ch_vol_pct    = { 100, 100, 100, 100, 100, 100 },
    .surround_room = SURROUND_ROOM_SMALL,
    .surround_mode = SURROUND_MODE_MUSIC
};

// Конвертація 0..100% у формат ADAU1452 8.24 (квадратична крива для плавності на слух)
static uint32_t pct_to_dsp_8_24(uint8_t pct) {
    if (pct == 0) return DSP_ZERO_8_24;
    if (pct >= 100) return DSP_UNITY_8_24;

    float norm = (float)pct / 100.0f;
    float gain = norm * norm;
    return (uint32_t)(gain * DSP_SCALE_8_24_FLOAT + 0.5f);
}

// ============================================================================
// Керування блоками ADAU1452
// ============================================================================

// 1. Сканер сигналу (4 ReadBack-блоки)
esp_err_t audio_manager_read_scanner(scanner_flags_t *out_flags) {
    if (!out_flags) return ESP_ERR_INVALID_ARG;

    uint32_t rl = 0, rr = 0, ble = 0, tos = 0;

    if (adau1452_read_u32(REG_READBACK_LINE_L,  &rl)  != ESP_OK) return ESP_FAIL;
    if (adau1452_read_u32(REG_READBACK_LINE_R,  &rr)  != ESP_OK) return ESP_FAIL;
    if (adau1452_read_u32(REG_READBACK_BLE,     &ble) != ESP_OK) return ESP_FAIL;
    if (adau1452_read_u32(REG_READBACK_TOSLINK, &tos) != ESP_OK) return ESP_FAIL;

    out_flags->line_l  = (rl  == DSP_READBACK_SIGNAL_HEX);
    out_flags->line_r  = (rr  == DSP_READBACK_SIGNAL_HEX);
    out_flags->ble     = (ble == DSP_READBACK_SIGNAL_HEX);
    out_flags->toslink = (tos == DSP_READBACK_SIGNAL_HEX);

    return ESP_OK;
}

// 2. Свіч псевдостерео Line (Nx2-1_5) — на майбутнє
esp_err_t audio_manager_set_line_mode(line_input_mode_t mode) {
    if (mode > LINE_MODE_MONO_R_ALL) return ESP_ERR_INVALID_ARG;
    s_dsp.line_mode = mode;
    return adau1452_set_slew_mux(REG_MUX_LINE_MODE_BASE, MUX_LINE_MODE_INPUTS, (uint8_t)mode);
}

// 3. Селектор входів (Nx2-1_3: 0=Analog, 1=BLE, 2=Optical)
esp_err_t audio_manager_set_input_source(audio_input_src_t src) {
    if (src > INPUT_SRC_OPTICAL) return ESP_ERR_INVALID_ARG;
    s_dsp.input_source = src;
    return adau1452_set_slew_mux(REG_MUX_INPUT_SEL_BASE, MUX_INPUT_SEL_INPUTS, (uint8_t)src);
}

// 4. 15-смуговий EQ (Mid Filter1)
esp_err_t audio_manager_set_eq_preset(eq_preset_id_t preset) {
    if (preset >= EQ_PRESET_MAX) return ESP_ERR_INVALID_ARG;
    s_dsp.eq_preset = (uint8_t)preset;
    return adau1452_apply_eq_preset(preset);
}

// 5. Режим 2.1 / 5.1 (Nx2-1_4 + автоматичний Mute1 для відсікання каналів у 2.1)
esp_err_t audio_manager_set_speaker_mode(speaker_mode_t mode) {
    if (mode > SPEAKER_MODE_5_1) return ESP_ERR_INVALID_ARG;
    s_dsp.speaker_mode = mode;

    esp_err_t ret;
    if (mode == SPEAKER_MODE_2_1) {
        ret = adau1452_write_u32(REG_MUTE_2_1_CUTOFF, DSP_ZERO_8_24);
        if (ret != ESP_OK) return ret;
        return adau1452_set_slew_mux(REG_MUX_SPEAKER_MODE_BASE, MUX_SPEAKER_MODE_INPUTS, 0);
    } else {
        ret = adau1452_set_slew_mux(REG_MUX_SPEAKER_MODE_BASE, MUX_SPEAKER_MODE_INPUTS, 1);
        if (ret != ESP_OK) return ret;
        return adau1452_write_u32(REG_MUTE_2_1_CUTOFF, DSP_UNITY_8_24);
    }
}

// 6. Затримки каналів (меню My Space)
esp_err_t audio_manager_set_channel_delay(audio_channel_t ch, float delay_ms) {
    if (ch >= CH_COUNT) return ESP_ERR_INVALID_ARG;

    if (delay_ms < 0.0f) delay_ms = 0.0f;
    if (delay_ms > k_max_delay_ms[ch]) delay_ms = k_max_delay_ms[ch];

    uint32_t samples = (uint32_t)(delay_ms * DSP_SAMPLE_RATE_KHZ + 0.5f);
    s_dsp.delays_ms[ch] = delay_ms;

    return adau1452_write_u32(k_delay_regs[ch], samples);
}

esp_err_t audio_manager_set_channel_distance_m(audio_channel_t ch, float dist_m) {
    if (ch >= CH_COUNT) return ESP_ERR_INVALID_ARG;
    if (dist_m < 0.0f) dist_m = 0.0f;
    if (dist_m > 10.0f) dist_m = 10.0f;

    // Переведення метрів у мс затримки (швидкість звуку ~343 м/с -> 1 м ≈ 2.91545 мс)
    float delay_ms = (dist_m / 343.0f) * 1000.0f;
    return audio_manager_set_channel_delay(ch, delay_ms);
}

// 7. Поканальні гучності 0..100% або -12..+12 дБ (меню My Space)
esp_err_t audio_manager_set_channel_volume(audio_channel_t ch, uint8_t pct) {
    if (ch >= CH_COUNT) return ESP_ERR_INVALID_ARG;
    if (pct > 100) pct = 100;

    s_dsp.ch_vol_pct[ch] = pct;
    return adau1452_write_u32(k_vol_regs[ch], pct_to_dsp_8_24(pct));
}

esp_err_t audio_manager_set_channel_gain_db(audio_channel_t ch, float gain_db) {
    if (ch >= CH_COUNT) return ESP_ERR_INVALID_ARG;
    if (gain_db < -12.0f) gain_db = -12.0f;
    if (gain_db >  12.0f) gain_db =  12.0f;

    uint32_t val_8_24;
    if (fabsf(gain_db) < 0.05f) {
        val_8_24 = DSP_UNITY_8_24; // Точна одиниця (0.0 дБ = 0x01000000)
    } else {
        float linear = powf(10.0f, gain_db / 20.0f);
        val_8_24 = (uint32_t)(linear * DSP_SCALE_8_24_FLOAT + 0.5f);
    }

    return adau1452_write_u32(k_vol_regs[ch], val_8_24);
}

// 8. Master-гучність 0..100% (Single 4_2)
esp_err_t audio_manager_set_master_volume(uint8_t pct) {
    if (pct > 100) pct = 100;

    master_volume = pct;
    return adau1452_write_u32(REG_VOL_MASTER, pct_to_dsp_8_24(pct));
}

// 9. Загальний Mute (Mute2 + Mute1)
esp_err_t audio_manager_set_master_mute(bool mute) {
    is_muted = mute;
    esp_err_t ret = adau1452_write_u32(REG_MUTE_MASTER, mute ? DSP_ZERO_8_24 : DSP_UNITY_8_24);
    if (ret != ESP_OK) return ret;

    uint32_t mute1_val = (mute || s_dsp.speaker_mode == SPEAKER_MODE_2_1) ? DSP_ZERO_8_24 : DSP_UNITY_8_24;
    return adau1452_write_u32(REG_MUTE_2_1_CUTOFF, mute1_val);
}

// 10. ADI Surround
esp_err_t audio_manager_set_surround_mode(surround_mode_t mode) {
    if (mode > SURROUND_MODE_SURROUND) return ESP_ERR_INVALID_ARG;
    s_dsp.surround_mode = (uint8_t)mode;
    return adau1452_apply_surround((surround_room_t)s_dsp.surround_room, mode);
}

esp_err_t audio_manager_set_surround_room(surround_room_t room) {
    if (room > SURROUND_ROOM_LARGE) return ESP_ERR_INVALID_ARG;
    s_dsp.surround_room = (uint8_t)room;
    return adau1452_apply_surround(room, (surround_mode_t)s_dsp.surround_mode);
}

const dsp_runtime_state_t* audio_manager_get_dsp_state(void) {
    return &s_dsp;
}

void audio_manager_apply_all_settings(void) {
    if (master_volume > 100) master_volume = 50;
    s_dsp.speaker_mode  = g_settings.sys_mode ? SPEAKER_MODE_5_1 : SPEAKER_MODE_2_1;
    s_dsp.eq_preset     = (g_settings.adau_eq_preset < EQ_PRESET_MAX) ? g_settings.adau_eq_preset : EQ_PRESET_NATURE;
    s_dsp.surround_mode = g_settings.adau_surround_mode ? SURROUND_MODE_MUSIC : SURROUND_MODE_SURROUND;

    audio_manager_set_line_mode(s_dsp.line_mode);
    audio_manager_set_input_source(s_dsp.input_source);
    audio_manager_set_speaker_mode(s_dsp.speaker_mode);
    audio_manager_set_master_volume(master_volume);
    audio_manager_set_eq_preset((eq_preset_id_t)s_dsp.eq_preset);
    audio_manager_set_surround_mode((surround_mode_t)s_dsp.surround_mode);

    for (int ch = 0; ch < CH_COUNT; ch++) {
        float dist_m = (float)g_settings.adau_distances[ch] / 10.0f;
        uint8_t raw_vol = g_settings.adau_vol_ch[ch];
        if (raw_vol > 48) raw_vol = 24; // Захист: 24 = 0.0 дБ
        float gain_db = ((float)raw_vol / 2.0f) - 12.0f;

        audio_manager_set_channel_distance_m((audio_channel_t)ch, dist_m);
        audio_manager_set_channel_gain_db((audio_channel_t)ch, gain_db);
    }

    ESP_LOGI(TAG, "Застосовано параметри DSP: Vol=%u%%, In=%d, Mode=%s, EQ=%u, Surr=%u, Mute=%d",
             master_volume,
             (int)s_dsp.input_source,
             (s_dsp.speaker_mode == SPEAKER_MODE_5_1) ? "5.1" : "2.1",
             (unsigned)s_dsp.eq_preset,
             (unsigned)s_dsp.surround_mode,
             (int)is_muted);
}

// ============================================================================
// Оновлення фільтрів AK5572 / AK4493 та запуск аудіотракту
// ============================================================================

void audio_update_filters(uint8_t ui_filter_index) {
    if (ui_filter_index > 5) return;

    ESP_LOGI(TAG, "Перемикання на пресет: %d. Активація Mute...", ui_filter_index);

    // 1. Приглушуємо звук перед зміною математики фільтрів (запобігає клацанню)
    adau1452_write_u32(REG_MUTE_MASTER, DSP_ZERO_8_24);
    vTaskDelay(pdMS_TO_TICKS(10)); // Даємо 10 мс на спад амплітуди

    // 2. Оновлюємо регістри
    uint8_t adc_filter = ak5572_filter_map[ui_filter_index];
    ak5572_set_filter(adc_filter); 
    
    uint8_t dac_filter = ak4493_filter_map[ui_filter_index];
    ak4493_set_filter_all(dac_filter); 

    // 3. Повертаємо звук (якщо користувач сам не вмикав Mute)
    vTaskDelay(pdMS_TO_TICKS(20)); // Даємо час ЦАПам зафіксувати нові налаштування
    if (!is_muted) {
        adau1452_write_u32(REG_MUTE_MASTER, DSP_UNITY_8_24);
    }

    ESP_LOGI(TAG, "Фільтри оновлено (AK5572: 0x%02X, AK4493: 0x%02X)", adc_filter, dac_filter);
}

void audio_manager_init_all(void) {
    ESP_LOGI(TAG, "Ініціалізація аудіотракту...");

    // Читаємо індекс обраного фільтра (перейменував на audio_preset для логіки)
    uint8_t saved_filter_idx = g_settings.audio_preset; 
    if (saved_filter_idx > 5) saved_filter_idx = 0; // Захист від сміття в EEPROM


    // ==============================================================
    // КРОК 1: Підключення I2C до DSP ADAU1452 (ядро та клоки працюють автономно)
    // ==============================================================
    ESP_LOGI(TAG, "Підключення до DSP ADAU1452...");
    if (adau1452_init(i2c_bus_handle, ADAU1452_DSP_ADDR) == ESP_OK) {
        adau1452_write_u32(REG_MUTE_MASTER, DSP_ZERO_8_24);
        audio_manager_apply_all_settings();
    }
    
    // Чекаємо стабілізації PLL всередині DSP
    vTaskDelay(pdMS_TO_TICKS(150)); 

    // ==============================================================
    // КРОК 2: Ініціалізація I2C AKM (Чипи залишаються в стані Reset)
    // ==============================================================
    ESP_LOGI(TAG, "Налаштування регістрів АЦП/ЦАП...");
    ak5572_init(i2c_bus_handle, AK5572_ADC_ADDR, ak5572_filter_map[saved_filter_idx]);
    ak4493_init_all(i2c_bus_handle, dac_addrs, ak4493_filter_map[saved_filter_idx]);

    // ==============================================================
    // КРОК 3: Зняття Reset з AKM (Клоки вже присутні!)
    // ==============================================================
    ESP_LOGI(TAG, "Клоки стабільні. Запуск модуляторів АЦП/ЦАП...");
    ak5572_start();                                 
    ak4493_start_all();

    if (!is_muted) {
        adau1452_write_u32(REG_MUTE_MASTER, DSP_UNITY_8_24);
    }

    ESP_LOGI(TAG, "Аудіотракт CATZILLA SYSTEM успішно запущено!");
}