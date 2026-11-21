#include "adau1452.h"

/* ============================================================================
 * 1. ПРЕСЕТИ 15-СМУГОВОГО ЕКВАЛАЙЗЕРА (Mid Filter1: FS = 192000 Гц, Q = 2.14, формат 8.24)
 *
 * ВАЖЛИВО ДЛЯ РУЧНОГО РЕДАГУВАННЯ ІЗ FilterTableGen:
 *   У FilterTableGen 5 рядків однієї точки йдуть у порядку:
 *     Рядок 1: B0
 *     Рядок 2: B1
 *     Рядок 3: B2
 *     Рядок 4: A1
 *     Рядок 5: A2
 *   Але блок Mid Filter1 (EQS300MultiDPHWSlew) у ADAU1452 приймає їх у порядку:
 *     { B2 (Рядок 3), B1 (Рядок 2), B0 (Рядок 1), A2 (Рядок 5), A1 (Рядок 4) }
 *   Плоский фільтр (0.0 дБ): { 0x00000000, 0x00000000, 0x01000000, 0x00000000, 0x00000000 }
 * ============================================================================ */
#define FLAT_BIQUAD { 0x00000000, 0x00000000, 0x01000000, 0x00000000, 0x00000000 }

// Автоматична таблиця стартових адрес усіх 15 смуг із adau1452_IC_1_PARAM.h
static const uint16_t k_eq_band_addrs[EQ_BANDS_COUNT] = {
    REG_EQ_BAND_1_START,  REG_EQ_BAND_2_START,  REG_EQ_BAND_3_START,
    REG_EQ_BAND_4_START,  REG_EQ_BAND_5_START,  REG_EQ_BAND_6_START,
    REG_EQ_BAND_7_START,  REG_EQ_BAND_8_START,  REG_EQ_BAND_9_START,
    REG_EQ_BAND_10_START, REG_EQ_BAND_11_START, REG_EQ_BAND_12_START,
    REG_EQ_BAND_13_START, REG_EQ_BAND_14_START, REG_EQ_BAND_15_START
};

// ============================================================================
// ОКРЕМІ ТАБЛИЦІ ПРЕСЕТІВ ЕКВАЛАЙЗЕРА (можна редагувати кожну смугу вручну)
// Формат кожного рядка: { B2, B1, B0, A2, A1 }
// ============================================================================

static const uint32_t eq_presets_table[EQ_PRESET_MAX][EQ_BANDS_COUNT][BIQUAD_WORDS_PER_BAND] = {

    // ==================== [0: POP] ====================
    [EQ_PRESET_POP] = {
        /* Смуга  1 (   25 Гц,  -2.0 дБ) */ { 0x00FFE6C9, 0xFE001C26, 0x00FFFD1C, 0xFF001C1B, 0x01FFE3DA },
        /* Смуга  2 (   40 Гц,  +1.5 дБ) */ { 0x00FFD7C6, 0xFE0024DF, 0x01000377, 0xFF0024C3, 0x01FFDB21 },
        /* Смуга  3 (   63 Гц,  +3.5 дБ) */ { 0x00FFBF9A, 0xFE0033E0, 0x01000CCD, 0xFF003399, 0x01FFCC20 },
        /* Смуга  4 (  100 Гц,  +4.5 дБ) */ { 0x00FF9876, 0xFE004E01, 0x01001A3C, 0xFF004D4D, 0x01FFB1FF },
        /* Смуга  5 (  160 Гц,  +3.5 дБ) */ { 0x00FF5C8D, 0xFE0084C0, 0x0100207E, 0xFF0082F5, 0x01FF7B40 },
        /* Смуга  6 (  250 Гц,  +1.5 дБ) */ { 0x00FF04F7, 0xFE00E9CB, 0x0100159F, 0xFF00E56A, 0x01FF1635 },
        /* Смуга  7 (  400 Гц,  +0.0 дБ) */ { 0x00000000, 0x00000000, 0x01000000, 0x00000000, 0x00000000 },
        /* Смуга  8 (  630 Гц,  -1.5 дБ) */ { 0x00FD89A1, 0xFE02C861, 0x00FFC9B4, 0xFF02ACAB, 0x01FD379F },
        /* Смуга  9 ( 1000 Гц,  -2.0 дБ) */ { 0x00FC17EE, 0xFE04A048, 0x00FF8D5E, 0xFF045AB3, 0x01FB5FB8 },
        /* Смуга 10 ( 1600 Гц,  -1.0 дБ) */ { 0x00F9CF1F, 0xFE073D58, 0x00FFA4DE, 0xFF068C03, 0x01F8C2A8 },
        /* Смуга 11 ( 2500 Гц,  +1.0 дБ) */ { 0x00F66197, 0xFE0ABF76, 0x01008D97, 0xFF0910D2, 0x01F5408A },
        /* Смуга 12 ( 4000 Гц,  +2.5 дБ) */ { 0x00F0A12D, 0xFE1170EC, 0x01023264, 0xFF0D2C6F, 0x01EE8F14 },
        /* Смуга 13 ( 6300 Гц,  +3.0 дБ) */ { 0x00E81B5A, 0xFE1E3B14, 0x010415EE, 0xFF13CEB8, 0x01E1C4EC },
        /* Смуга 14 (10000 Гц,  +2.0 дБ) */ { 0x00DBB87E, 0xFE399687, 0x01042890, 0xFF201EF1, 0x01C66979 },
        /* Смуга 15 (16000 Гц,  +1.0 дБ) */ { 0x00CA09FD, 0xFE70A3B4, 0x01031A52, 0xFF32DBB1, 0x018F5C4C }
    },

    // ==================== [1: ROCK] ====================
    [EQ_PRESET_ROCK] = {
        /* Смуга  1 (   25 Гц,  +5.0 дБ) */ { 0x00FFE5E8, 0xFE0012D4, 0x0100074F, 0xFF0012C9, 0x01FFED2C },
        /* Смуга  2 (   40 Гц,  +4.5 дБ) */ { 0x00FFD692, 0xFE001F0B, 0x01000A80, 0xFF001EEE, 0x01FFE0F5 },
        /* Смуга  3 (   63 Гц,  +3.5 дБ) */ { 0x00FFBF9A, 0xFE0033E0, 0x01000CCD, 0xFF003399, 0x01FFCC20 },
        /* Смуга  4 (  100 Гц,  +1.5 дБ) */ { 0x00FF9B7B, 0xFE005C90, 0x010008A8, 0xFF005BDD, 0x01FFA370 },
        /* Смуга  5 (  160 Гц,  -1.5 дБ) */ { 0x00FF5F45, 0xFE00B05F, 0x00FFF228, 0xFF00AE93, 0x01FF4FA1 },
        /* Смуга  6 (  250 Гц,  -4.0 дБ) */ { 0x00FEFF68, 0xFE013F08, 0x00FFC5F0, 0xFF013AA8, 0x01FEC0F8 },
        /* Смуга  7 (  400 Гц,  -5.0 дБ) */ { 0x00FE601E, 0xFE021F91, 0x00FF8B80, 0xFF021462, 0x01FDE06F },
        /* Смуга  8 (  630 Гц,  -3.5 дБ) */ { 0x00FD7F97, 0xFE031B6A, 0x00FF80B1, 0xFF02FFB9, 0x01FCE496 },
        /* Смуга  9 ( 1000 Гц,  -1.0 дБ) */ { 0x00FC1C67, 0xFE046276, 0x00FFC6C1, 0xFF041CD8, 0x01FB9D8A },
        /* Смуга 10 ( 1600 Гц,  +1.5 дБ) */ { 0x00F9C91D, 0xFE065F7F, 0x01008907, 0xFF05ADDC, 0x01F9A081 },
        /* Смуга 11 ( 2500 Гц,  +3.5 дБ) */ { 0x00F62DA2, 0xFE098E32, 0x0101F3D7, 0xFF07DE87, 0x01F671CE },
        /* Смуга 12 ( 4000 Гц,  +5.0 дБ) */ { 0x00F0191C, 0xFE0FBAC8, 0x01047461, 0xFF0B7283, 0x01F04538 },
        /* Смуга 13 ( 6300 Гц,  +6.0 дБ) */ { 0x00E6E2F1, 0xFE1B41A7, 0x0108583D, 0xFF10C4D1, 0x01E4BE59 },
        /* Смуга 14 (10000 Гц,  +6.0 дБ) */ { 0x00D94A08, 0xFE33A5E7, 0x010CDCE0, 0xFF19D919, 0x01CC5A19 },
        /* Смуга 15 (16000 Гц,  +5.5 дБ) */ { 0x00C617CD, 0xFE676068, 0x0111BEA5, 0xFF28298E, 0x01989F98 }
    },

    // ==================== [2: JAZZ] ====================
    [EQ_PRESET_JAZZ] = {
        /* Смуга  1 (   25 Гц,  +3.5 дБ) */ { 0x00FFE670, 0xFE001486, 0x01000515, 0xFF00147B, 0x01FFEB7A },
        /* Смуга  2 (   40 Гц,  +3.0 дБ) */ { 0x00FFD753, 0xFE0021D5, 0x010006F5, 0xFF0021B8, 0x01FFDE2B },
        /* Смуга  3 (   63 Гц,  +2.0 дБ) */ { 0x00FFC079, 0xFE003886, 0x01000748, 0xFF00383F, 0x01FFC77A },
        /* Смуга  4 (  100 Гц,  +0.5 дБ) */ { 0x00FF9BD1, 0xFE006201, 0x010002E2, 0xFF00614D, 0x01FF9DFF },
        /* Смуга  5 (  160 Гц,  -0.5 дБ) */ { 0x00FF5FCA, 0xFE00A69E, 0x00FFFB64, 0xFF00A4D3, 0x01FF5962 },
        /* Смуга  6 (  250 Гц,  +0.0 дБ) */ { 0x00000000, 0x00000000, 0x01000000, 0x00000000, 0x00000000 },
        /* Смуга  7 (  400 Гц,  -1.0 дБ) */ { 0x00FE6FC6, 0xFE01B26F, 0x00FFE8FD, 0xFF01A73E, 0x01FE4D91 },
        /* Смуга  8 (  630 Гц,  +0.0 дБ) */ { 0x00000000, 0x00000000, 0x01000000, 0x00000000, 0x00000000 },
        /* Смуга  9 ( 1000 Гц,  +1.0 дБ) */ { 0x00FC1B88, 0xFE03F0D8, 0x0100394C, 0xFF03AB2C, 0x01FC0F28 },
        /* Смуга 10 ( 1600 Гц,  +2.0 дБ) */ { 0x00F9C402, 0xFE0636BF, 0x0100B6F0, 0xFF05850E, 0x01F9C941 },
        /* Смуга 11 ( 2500 Гц,  +3.0 дБ) */ { 0x00F63C1A, 0xFE09C7E6, 0x0101AB7A, 0xFF08186D, 0x01F6381A },
        /* Смуга 12 ( 4000 Гц,  +3.5 дБ) */ { 0x00F0748A, 0xFE10BA6F, 0x01031717, 0xFF0C745F, 0x01EF4591 },
        /* Смуга 13 ( 6300 Гц,  +4.0 дБ) */ { 0x00E7C78E, 0xFE1D2F98, 0x01057AFF, 0xFF12BD73, 0x01E2D068 },
        /* Смуга 14 (10000 Гц,  +4.5 дБ) */ { 0x00DA6D8A, 0xFE35BC08, 0x0109854D, 0xFF1C0D29, 0x01CA43F8 },
        /* Смуга 15 (16000 Гц,  +4.5 дБ) */ { 0x00C7485C, 0xFE69445F, 0x010E5F40, 0xFF2A5864, 0x0196BBA1 }
    },

    // ==================== [3: SYMPH] ====================
    [EQ_PRESET_SYMPH] = {
        /* Смуга  1 (   25 Гц,  +6.0 дБ) */ { 0x00FFE571, 0xFE0011C7, 0x010008D3, 0xFF0011BC, 0x01FFEE39 },
        /* Смуга  2 (   40 Гц,  +6.0 дБ) */ { 0x00FFD582, 0xFE001C7C, 0x01000E1F, 0xFF001C60, 0x01FFE384 },
        /* Смуга  3 (   63 Гц,  +4.5 дБ) */ { 0x00FFBEC2, 0xFE0030FD, 0x01001088, 0xFF0030B6, 0x01FFCF03 },
        /* Смуга  4 (  100 Гц,  +3.5 дБ) */ { 0x00FF99CE, 0xFE005295, 0x01001451, 0xFF0051E1, 0x01FFAD6B },
        /* Смуга  5 (  160 Гц,  +3.0 дБ) */ { 0x00FF5D6C, 0xFE008892, 0x01001BCD, 0xFF0086C7, 0x01FF776E },
        /* Смуга  6 (  250 Гц,  +1.5 дБ) */ { 0x00FF04F7, 0xFE00E9CB, 0x0100159F, 0xFF00E56A, 0x01FF1635 },
        /* Смуга  7 (  400 Гц,  +0.0 дБ) */ { 0x00000000, 0x00000000, 0x01000000, 0x00000000, 0x00000000 },
        /* Смуга  8 (  630 Гц,  -2.0 дБ) */ { 0x00FD87E6, 0xFE02DC42, 0x00FFB78C, 0xFF02C08E, 0x01FD23BE },
        /* Смуга  9 ( 1000 Гц,  -3.5 дБ) */ { 0x00FC0B11, 0xFE0503D3, 0x00FF36A3, 0xFF04BE4C, 0x01FAFC2D },
        /* Смуга 10 ( 1600 Гц,  -3.5 дБ) */ { 0x00F9B4B5, 0xFE083C97, 0x00FEBFB0, 0xFF078B9B, 0x01F7C369 },
        /* Смуга 11 ( 2500 Гц,  -3.0 дБ) */ { 0x00F64C4D, 0xFE0D0915, 0x00FE574B, 0xFF0B5C68, 0x01F2F6EB },
        /* Смуга 12 ( 4000 Гц,  -1.5 дБ) */ { 0x00F0D4EB, 0xFE14B6CD, 0x00FEB18A, 0xFF10798B, 0x01EB4933 },
        /* Смуга 13 ( 6300 Гц,  +0.0 дБ) */ { 0x00000000, 0x00000000, 0x01000000, 0x00000000, 0x00000000 },
        /* Смуга 14 (10000 Гц,  +0.5 дБ) */ { 0x00DC25D0, 0xFE3C24FF, 0x01010819, 0xFF22D217, 0x01C3DB01 },
        /* Смуга 15 (16000 Гц,  +0.5 дБ) */ { 0x00CA4308, 0xFE71CB63, 0x01018BD9, 0xFF34311F, 0x018E349D }
    },

    // ==================== [4: NATURE (Flat 0.0 dB)] ====================
    [EQ_PRESET_NATURE] = {
        /* Смуга  1 (   25 Гц,  +0.0 дБ) */ FLAT_BIQUAD,
        /* Смуга  2 (   40 Гц,  +0.0 дБ) */ FLAT_BIQUAD,
        /* Смуга  3 (   63 Гц,  +0.0 дБ) */ FLAT_BIQUAD,
        /* Смуга  4 (  100 Гц,  +0.0 дБ) */ FLAT_BIQUAD,
        /* Смуга  5 (  160 Гц,  +0.0 дБ) */ FLAT_BIQUAD,
        /* Смуга  6 (  250 Гц,  +0.0 дБ) */ FLAT_BIQUAD,
        /* Смуга  7 (  400 Гц,  +0.0 дБ) */ FLAT_BIQUAD,
        /* Смуга  8 (  630 Гц,  +0.0 дБ) */ FLAT_BIQUAD,
        /* Смуга  9 ( 1000 Гц,  +0.0 дБ) */ FLAT_BIQUAD,
        /* Смуга 10 ( 1600 Гц,  +0.0 дБ) */ FLAT_BIQUAD,
        /* Смуга 11 ( 2500 Гц,  +0.0 дБ) */ FLAT_BIQUAD,
        /* Смуга 12 ( 4000 Гц,  +0.0 дБ) */ FLAT_BIQUAD,
        /* Смуга 13 ( 6300 Гц,  +0.0 дБ) */ FLAT_BIQUAD,
        /* Смуга 14 (10000 Гц,  +0.0 дБ) */ FLAT_BIQUAD,
        /* Смуга 15 (16000 Гц,  +0.0 дБ) */ FLAT_BIQUAD
    },

    // ==================== [5: B&T (Bass & Treble)] ====================
    [EQ_PRESET_BNT] = {
        /* Смуга  1 (   25 Гц,  +6.0 дБ) */ { 0x00FFE571, 0xFE0011C7, 0x010008D3, 0xFF0011BC, 0x01FFEE39 },
        /* Смуга  2 (   40 Гц,  +5.5 дБ) */ { 0x00FFD5E5, 0xFE001D50, 0x01000CE7, 0xFF001D34, 0x01FFE2B0 },
        /* Смуга  3 (   63 Гц,  +4.5 дБ) */ { 0x00FFBEC2, 0xFE0030FD, 0x01001088, 0xFF0030B6, 0x01FFCF03 },
        /* Смуга  4 (  100 Гц,  +3.0 дБ) */ { 0x00FF9A5A, 0xFE0054F8, 0x01001162, 0xFF005445, 0x01FFAB08 },
        /* Смуга  5 (  160 Гц,  +1.0 дБ) */ { 0x00FF5F92, 0xFE0098FF, 0x0100093A, 0xFF009734, 0x01FF6701 },
        /* Смуга  6 (  250 Гц,  -1.5 дБ) */ { 0x00FF050C, 0xFE0114F2, 0x00FFEA62, 0xFF011092, 0x01FEEB0E },
        /* Смуга  7 (  400 Гц,  -3.5 дБ) */ { 0x00FE6880, 0xFE01F3B2, 0x00FFAEFE, 0xFF01E882, 0x01FE0C4E },
        /* Смуга  8 (  630 Гц,  -4.5 дБ) */ { 0x00FD776C, 0xFE03489C, 0x00FF5BA7, 0xFF032CEE, 0x01FCB764 },
        /* Смуга  9 ( 1000 Гц,  -3.5 дБ) */ { 0x00FC0B11, 0xFE0503D3, 0x00FF36A3, 0xFF04BE4C, 0x01FAFC2D },
        /* Смуга 10 ( 1600 Гц,  -1.5 дБ) */ { 0x00F9CC6F, 0xFE076D93, 0x00FF7742, 0xFF06BC4F, 0x01F8926D },
        /* Смуга 11 ( 2500 Гц,  +1.0 дБ) */ { 0x00F66197, 0xFE0ABF76, 0x01008D97, 0xFF0910D2, 0x01F5408A },
        /* Смуга 12 ( 4000 Гц,  +2.5 дБ) */ { 0x00F0A12D, 0xFE1170EC, 0x01023264, 0xFF0D2C6F, 0x01EE8F14 },
        /* Смуга 13 ( 6300 Гц,  +4.0 дБ) */ { 0x00E7C78E, 0xFE1D2F98, 0x01057AFF, 0xFF12BD73, 0x01E2D068 },
        /* Смуга 14 (10000 Гц,  +5.0 дБ) */ { 0x00DA144E, 0xFE350563, 0x010A9F6B, 0xFF1B4C47, 0x01CAFA9D },
        /* Смуга 15 (16000 Гц,  +6.0 дБ) */ { 0x00C56D86, 0xFE667714, 0x01137659, 0xFF271C21, 0x019988EC }
    }
};

/* ============================================================================
 * 2. ПРЕСЕТИ ADI SURROUND (ADIsurround1: 5 активних регістрів перемикання)
 * Згідно з Capture Window SigmaStudio:
 *   0x00C8 (200) - ADIsurroundAlgS3001gain1
 *   0x00C9 (201) - ADIsurroundAlgS3001gain2
 *   0x00CA (202) - ADIsurroundAlgS3001gain3
 *   0x00CB (203) - ADIsurroundAlgS3001gain4
 *   0x00B9 (185) - ADIsurroundAlgS3001dil
 * ============================================================================ */
#define SURROUND_ACTIVE_REGS_COUNT 5

static const uint16_t k_surround_addrs[SURROUND_ACTIVE_REGS_COUNT] = {
    MOD_ADISURROUND1_ALG0_ADISURROUNDALGS3001GAIN1_ADDR, // 0x00C8 (200)
    MOD_ADISURROUND1_ALG0_ADISURROUNDALGS3001GAIN2_ADDR, // 0x00C9 (201)
    MOD_ADISURROUND1_ALG0_ADISURROUNDALGS3001GAIN3_ADDR, // 0x00CA (202)
    MOD_ADISURROUND1_ALG0_ADISURROUNDALGS3001GAIN4_ADDR, // 0x00CB (203)
    MOD_ADISURROUND1_ALG0_ADISURROUNDALGS3001DIL_ADDR    // 0x00B9 (185)
};

// Режим SURROUND (17:04:40 із Capture Window)
#define SURROUND_PRESET_SURROUND_DATA { \
    0x00B53BEF, /* gain1 (0x00C8) = 0.70794576 */ \
    0x00804DCE, /* gain2 (0x00C9) = 0.50118720 */ \
    0x00404DE6, /* gain3 (0x00CA) = 0.25118863 */ \
    0x00B53BEF, /* gain4 (0x00CB) = 0.70794576 */ \
    0x00000000  /* dil   (0x00B9) = 0          */ \
}

// Режим MUSIC (17:04:44 із Capture Window)
#define SURROUND_PRESET_MUSIC_DATA { \
    0x00B53BEF, /* gain1 (0x00C8) = 0.70794576 */ \
    0x002892C2, /* gain2 (0x00C9) = 0.15848934 */ \
    0x00404DE6, /* gain3 (0x00CA) = 0.25118863 */ \
    0x00804DCE, /* gain4 (0x00CB) = 0.50118720 */ \
    0x00000000  /* dil   (0x00B9) = 0          */ \
}

static const uint32_t surround_presets_table[2][2][SURROUND_ACTIVE_REGS_COUNT] = {
    [SURROUND_ROOM_SMALL] = {
        [SURROUND_MODE_MUSIC]    = SURROUND_PRESET_MUSIC_DATA,
        [SURROUND_MODE_SURROUND] = SURROUND_PRESET_SURROUND_DATA
    },
    [SURROUND_ROOM_LARGE] = {
        [SURROUND_MODE_MUSIC]    = SURROUND_PRESET_MUSIC_DATA,
        [SURROUND_MODE_SURROUND] = SURROUND_PRESET_SURROUND_DATA
    }
};

esp_err_t adau1452_apply_eq_preset(eq_preset_id_t preset) {
    if (preset >= EQ_PRESET_MAX) return ESP_ERR_INVALID_ARG;

    // Mid Filter1 (EQS300MultiDPHWSlew) має власний апаратний Slew (SLEWMODE = 0x208A)
    // і цільові регістри TARGB2.. у банках DM0 (парні смуги) та DM1 (непарні смуги)
    for (uint8_t band = 0; band < EQ_BANDS_COUNT; band++) {
        uint16_t base = k_eq_band_addrs[band];
        for (uint8_t w = 0; w < BIQUAD_WORDS_PER_BAND; w++) {
            esp_err_t ret = adau1452_write_u32(base + w, eq_presets_table[preset][band][w]);
            if (ret != ESP_OK) return ret;
        }
    }
    return ESP_OK;
}

esp_err_t adau1452_apply_surround(surround_room_t room, surround_mode_t mode) {
    if (room > SURROUND_ROOM_LARGE || mode > SURROUND_MODE_SURROUND) {
        return ESP_ERR_INVALID_ARG;
    }

    for (uint8_t i = 0; i < SURROUND_ACTIVE_REGS_COUNT; i++) {
        esp_err_t ret = adau1452_write_u32(
            k_surround_addrs[i],
            surround_presets_table[room][mode][i]
        );
        if (ret != ESP_OK) return ret;
    }
    return ESP_OK;
}
