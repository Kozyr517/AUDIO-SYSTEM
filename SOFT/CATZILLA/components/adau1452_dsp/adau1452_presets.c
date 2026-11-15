#include "adau1452.h"

// Одиничний біквад (Flat): {B0, B1, B2, A1, A2}
#define FLAT_BIQUAD { 0x01000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 }

static const uint32_t eq_presets_table[EQ_PRESET_MAX][EQ_BANDS_COUNT][BIQUAD_WORDS_PER_BAND] = {
    [EQ_PRESET_FLAT] = {
        FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD,
        FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD,
        FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD
    },
    [EQ_PRESET_ROCK] = {
        FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD,
        FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD,
        FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD
    },
    [EQ_PRESET_POP] = {
        FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD,
        FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD,
        FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD
    },
    [EQ_PRESET_JAZZ] = {
        FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD,
        FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD,
        FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD
    },
    [EQ_PRESET_CUSTOM] = {
        FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD,
        FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD,
        FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD, FLAT_BIQUAD
    }
};

static const uint32_t surround_presets_table[2][2][SURROUND_PARAMS_COUNT] = {
    [SURROUND_ROOM_SMALL] = {
        [SURROUND_STYLE_MUSIC] = { 0x01000000, 0, 0, 0, 0, 0, 0, 0 },
        [SURROUND_STYLE_MOVIE] = { 0x01000000, 0, 0, 0, 0, 0, 0, 0 }
    },
    [SURROUND_ROOM_HALL] = {
        [SURROUND_STYLE_MUSIC] = { 0x01000000, 0, 0, 0, 0, 0, 0, 0 },
        [SURROUND_STYLE_MOVIE] = { 0x01000000, 0, 0, 0, 0, 0, 0, 0 }
    }
};

esp_err_t adau1452_apply_eq_preset(eq_preset_id_t preset) {
    if (preset >= EQ_PRESET_MAX) return ESP_ERR_INVALID_ARG;

    for (uint8_t band = 0; band < EQ_BANDS_COUNT; band++) {
        uint16_t band_addr = REG_EQ_BAND1_START + (band * BIQUAD_WORDS_PER_BAND);
        esp_err_t ret = adau1452_safeload_write(
            band_addr,
            eq_presets_table[preset][band],
            BIQUAD_WORDS_PER_BAND
        );
        if (ret != ESP_OK) return ret;
    }
    return ESP_OK;
}

esp_err_t adau1452_apply_surround(surround_room_t room, surround_style_t style) {
    if (room > SURROUND_ROOM_HALL || style > SURROUND_STYLE_MOVIE) {
        return ESP_ERR_INVALID_ARG;
    }

    for (uint8_t i = 0; i < SURROUND_PARAMS_COUNT; i++) {
        esp_err_t ret = adau1452_write_u32(REG_SURROUND_BASE + i, surround_presets_table[room][style][i]);
        if (ret != ESP_OK) return ret;
    }
    return ESP_OK;
}