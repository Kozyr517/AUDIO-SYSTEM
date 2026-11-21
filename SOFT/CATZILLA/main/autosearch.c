#include <stdio.h>
#include "autosearch.h"
#include "audio_manager.h"
#include "driver/gpio.h"
#include "eeprom_24lc128.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "AUTOSEARCH";

// ==================== НАЛАШТУВАННЯ ТАЙМІНГІВ ====================
#define SCAN_STEP_WAIT_SEC        3     // Час прослуховування одного положення реле (2с DSP + 1с запас)
#define SIGNAL_LOSS_HOLD_SEC      5     // Додаткове утримання активного каналу при паузі між треками (+3с до 2с у DSP = 5с)
#define SCAN_TOTAL_TIME_SEC       60    // Загальний час активного пошуку реле (1 хвилина = 60 сек)
#define PHONO_EEPROM_SAVE_SEC     3600  // Автозапис у EEPROM кожну годину (3600 сек = 60 хв)

// ==================== ВНУТРІШНІ ЗМІННІ АВТОПОШУКУ ====================
static autosearch_state_t s_state             = AS_STATE_SCAN;
static line_input_mode_t  s_line_mode         = (line_input_mode_t)255;
static audio_input_src_t  s_input_src         = (audio_input_src_t)255;

static uint8_t            s_relay_gpio        = 255; // Початкове невалідне значення для гарантованого перемикання при ініціалізації
static uint8_t            s_locked_gpio       = RELAY_GPIO_VINYL;
static bool               s_has_locked_gpio   = false;

static uint8_t            s_step_timer        = 0;
static uint8_t            s_total_scan_timer  = 0;
static uint8_t            s_relay_settle_sec  = 0; // Таймер усталення реле (чекаємо 3с після старту/перемикання GPIO9)
static uint8_t            s_loss_hold_timer   = 0; // Лічильник утримання каналу під час паузи між треками
static bool               s_adc_signal_now    = false;

// Тимчасова індикація активного каналу на дисплеї (2 секунди)
#define ACTIVE_CH_POPUP_MS        2000
static const char        *s_popup_text        = NULL;
static int64_t            s_popup_until_ms    = 0;

// ==================== ЄДИНИЙ ЛІЧИЛЬНИК ЧАСУ ВІНІЛУ ====================
static uint32_t           s_needle_min        = 0; // Загальний час поточної голки (у хвилинах)
static uint32_t           s_total_min         = 0; // Загальний час програвача (у хвилинах, завжди >= s_needle_min)
static uint16_t           s_sec_accum         = 0; // Секундний акумулятор (0..59 сек -> +1 хв в обидва таймери)
static uint16_t           s_unsaved_min       = 0; // Скільки хвилин набігло з моменту останнього запису в EEPROM

// ============================================================================
// БЛОК ІНТЕГРАЦІЇ З AUDIO_MANAGER, GPIO9 ТА EEPROM
// ============================================================================

static void hw_eeprom_load_phono_timers(void) {
    s_needle_min = g_settings.phono_needle_minutes;
    s_total_min  = g_settings.phono_total_minutes;

    if (s_needle_min == 0xFFFFFFFFUL) s_needle_min = 0;
    if (s_total_min  == 0xFFFFFFFFUL) s_total_min  = 0;

    // Залізне правило: Total Time не може бути меншим за Needle Time!
    if (s_total_min < s_needle_min) {
        s_total_min = s_needle_min;
    }

    // ESP_LOGI(TAG, "Phono Timers Loaded: Needle=%lu min, Total=%lu min",
    //          (unsigned long)s_needle_min, (unsigned long)s_total_min);
}

static void hw_eeprom_save_phono_timers(void) {
    if (s_total_min < s_needle_min) {
        s_total_min = s_needle_min;
    }

    g_settings.phono_needle_minutes = s_needle_min;
    g_settings.phono_total_minutes  = s_total_min;
    settings_commit_check();

    s_unsaved_min = 0;

    // ESP_LOGI(TAG, "EEPROM SAVED -> Needle: %lu min, Total: %lu min",
    //          (unsigned long)s_needle_min, (unsigned long)s_total_min);
}

static void hw_set_relay_gpio(uint8_t level) {
    if (s_relay_gpio == level) return;
    s_relay_gpio = level;
    s_relay_settle_sec = 0; // Після перемикання реле чекаємо 3 сек, поки в DSP спаде перехідний процес (2с)

    gpio_set_level(RELAY_GPIO_PIN, level);

    // ESP_LOGI(TAG, "Relay GPIO9 switched to: %d (%s)",
    //          level, (level == RELAY_GPIO_VINYL) ? "VINYL" : "LINE IN");
}

static void hw_set_dsp_mux1(line_input_mode_t mode) {
    if (s_line_mode == mode) return;
    s_line_mode = mode;

    audio_manager_set_line_mode(mode);

    // ESP_LOGI(TAG, "DSP MUX1 (Line Mode) set to: %d", (int)mode);
}

static void hw_set_dsp_mux2(audio_input_src_t source) {
    if (s_input_src == source) return;
    s_input_src = source;

    audio_manager_set_input_source(source);

    // ESP_LOGI(TAG, "DSP MUX2 (Input Source) set to: %d", (int)source);
}

static void hw_read_scanner_flags(scanner_flags_t *flags) {
    if (audio_manager_read_scanner(flags) != ESP_OK) {
        flags->line_l  = false;
        flags->line_r  = false;
        flags->ble     = false;
        flags->toslink = false;
    }
}

// ============================================================================
// ЛОГІКА ЄДИНОГО ЛІЧИЛЬНИКА ВІНІЛУ ТА КОМАНД СКИДАННЯ
// ============================================================================

static void phono_timer_tick_1s(void) {
    if (!autosearch_is_phono_playing()) {
        return;
    }

    s_sec_accum++;

    if (s_sec_accum >= 60) {
        s_sec_accum = 0;

        s_needle_min++;
        s_total_min++;
        s_unsaved_min++;

        if (s_total_min < s_needle_min) {
            s_total_min = s_needle_min;
        }

        // Синхронізуємо у RAM-структуру g_settings, щоб при виході з меню або сні теж зберігалося
        g_settings.phono_needle_minutes = s_needle_min;
        g_settings.phono_total_minutes  = s_total_min;

        if (s_unsaved_min >= (PHONO_EEPROM_SAVE_SEC / 60)) {
            ESP_LOGI(TAG, "1 hour of vinyl playback reached -> Saving to EEPROM");
            hw_eeprom_save_phono_timers();
        }
    }
}

void phono_save_now(void) {
    if (s_unsaved_min > 0) {
        hw_eeprom_save_phono_timers();
    }
}

void phono_reset_needle_time(void) {
    s_needle_min = 0;
    s_sec_accum  = 0;

    hw_eeprom_save_phono_timers();
    ESP_LOGW(TAG, "CMD: Needle Time reset to 0! Total Time remains %lu min.", (unsigned long)s_total_min);
}

void phono_reset_total_time(void) {
    s_total_min = s_needle_min;

    hw_eeprom_save_phono_timers();
    ESP_LOGW(TAG, "CMD: Total Time reset to match Needle Time (%lu min)!", (unsigned long)s_total_min);
}

static void format_phono_time(uint32_t total_minutes, char *buf, uint8_t buf_size) {
    uint32_t total_hours = total_minutes / 60;
    uint32_t mins        = total_minutes % 60;

    uint32_t thousands   = (total_hours / 1000) % 1000;
    uint32_t hours_rem   = total_hours % 1000;

    snprintf(buf, buf_size, "%03lu.%03luh%02lum",
             (unsigned long)thousands,
             (unsigned long)hours_rem,
             (unsigned long)mins);
}

void phono_get_needle_time_str(char *buf, uint8_t buf_size) {
    format_phono_time(s_needle_min, buf, buf_size);
}

void phono_get_total_time_str(char *buf, uint8_t buf_size) {
    format_phono_time(s_total_min, buf, buf_size);
}

// ============================================================================
// ДОПОМІЖНА ЛОГІКА НАЛАШТУВАННЯ MUX1 (СТЕРЕО / ПСЕВДОСТЕРЕО)
// ============================================================================

static void update_mux1_from_adc(bool l_has_sig, bool r_has_sig) {
    if (l_has_sig && r_has_sig) {
        hw_set_dsp_mux1(LINE_MODE_STEREO);     // 0 = Стандартне стерео L/R
    } else if (l_has_sig && !r_has_sig) {
        hw_set_dsp_mux1(LINE_MODE_MONO_L_ALL); // 1 = Лівий дублюється на обидва (L->L+R)
    } else if (!l_has_sig && r_has_sig) {
        hw_set_dsp_mux1(LINE_MODE_MONO_R_ALL); // 2 = Правий дублюється на обидва (R->L+R)
    }
}

// ============================================================================
// ОСНОВНІ ФУНКЦІЇ АВТОПОШУКУ
// ============================================================================

void autosearch_init(void) {
    s_state            = AS_STATE_SCAN;
    s_step_timer       = 0;
    s_total_scan_timer = 0;
    s_relay_settle_sec = 0;
    s_loss_hold_timer  = 0;
    s_locked_gpio      = RELAY_GPIO_VINYL;
    s_has_locked_gpio  = false;
    s_adc_signal_now   = false;
    s_sec_accum        = 0;
    s_unsaved_min      = 0;
    s_relay_gpio       = 255;

    const dsp_runtime_state_t *st = audio_manager_get_dsp_state();
    if (st) {
        s_line_mode = st->line_mode;
        s_input_src = st->input_source;
    }

    hw_eeprom_load_phono_timers();
    hw_set_relay_gpio(RELAY_GPIO_VINYL); // Стартуємо з GPIO9 = 0 (VINYL)
}

void autosearch_force_scan_from_button(void) {
    if (s_adc_signal_now) {
        // ESP_LOGI(TAG, "Button 3 short click ignored: Analog signal is currently active.");
        return;
    }

    // ESP_LOGI(TAG, "Manual 60s scan started by Button 3 short click!");

    hw_set_relay_gpio(RELAY_GPIO_VINYL); // Починаємо сканування з пріоритету №1: GPIO9 = 0 (VINYL)
    s_state            = AS_STATE_SCAN;
    s_step_timer       = 0;
    s_total_scan_timer = 0;
}

void autosearch_tick_1s(void) {
    static int64_t last_tick_ms = 0;
    static const char *s_last_logged_channel = NULL;
    int64_t now_ms = esp_timer_get_time() / 1000;
    if (now_ms - last_tick_ms < 1000) {
        return;
    }
    last_tick_ms = now_ms;

    // Лічильник усталення після перемикання реле або старту (2с TimeConstant у DSP + 1с запас = 3с)
    if (s_relay_settle_sec < SCAN_STEP_WAIT_SEC) {
        s_relay_settle_sec++;
    }
    bool adc_settled = (s_relay_settle_sec >= SCAN_STEP_WAIT_SEC);

    // 1. Читаємо прапорці з audio_manager_read_scanner()
    scanner_flags_t flags;
    hw_read_scanner_flags(&flags);

    // Сигнал з АЦП вважаємо валідним лише після усталення реле (щоб клацання реле не викликало хибне спрацювання)
    bool adc_raw_sig = (flags.line_l || flags.line_r);
    bool adc_has_sig = adc_settled && adc_raw_sig;
    s_adc_signal_now = adc_has_sig;

    bool vinyl_sig   = (s_relay_gpio == RELAY_GPIO_VINYL)   && adc_has_sig;
    bool line_in_sig = (s_relay_gpio == RELAY_GPIO_LINE_IN) && adc_has_sig;

    // ESP_LOGI(TAG, "Scan[st=%d gpio9=%d settle=%u/%u]: ADC_L=%d ADC_R=%d (valid=%d) | BLE=%d | OPT=%d -> active=%s",
    //          (int)s_state, (int)s_relay_gpio, (unsigned)s_relay_settle_sec, (unsigned)SCAN_STEP_WAIT_SEC,
    //          (int)flags.line_l, (int)flags.line_r, (int)adc_has_sig,
    //          (int)flags.ble, (int)flags.toslink,
    //          autosearch_get_display_text());

    // ------------------------------------------------------------------------
    // ПРІОРИТЕТ №1: АНАЛОГ GPIO9 = 0 (VINYL)
    // ------------------------------------------------------------------------
    if (vinyl_sig) {
        update_mux1_from_adc(flags.line_l, flags.line_r);
        hw_set_dsp_mux2(INPUT_SRC_ANALOG);

        s_locked_gpio      = RELAY_GPIO_VINYL;
        s_has_locked_gpio  = true;

        s_state            = AS_STATE_PLAY_ADC;
        s_step_timer       = 0;
        s_total_scan_timer = 0;
        s_loss_hold_timer  = 0;
    }
    // ------------------------------------------------------------------------
    // ПРІОРИТЕТ №2: BLUETOOTH (BLE)
    // Якщо грає LINE IN (GPIO9=1) і між треками коротка пауза (триває утримання),
    // але на BLE з'явився реальний сигнал — перемикаємося на BLE одразу!
    // ------------------------------------------------------------------------
    else if (flags.ble) {
        uint8_t park_gpio = s_has_locked_gpio ? s_locked_gpio : RELAY_GPIO_VINYL;
        hw_set_relay_gpio(park_gpio);

        hw_set_dsp_mux2(INPUT_SRC_BLE);
        s_state            = AS_STATE_PLAY_DIG;
        s_step_timer       = 0;
        s_total_scan_timer = 0;
        s_loss_hold_timer  = 0;
    }
    // ------------------------------------------------------------------------
    // ПРІОРИТЕТ №3: ОПТИКА (TOSLINK)
    // ------------------------------------------------------------------------
    else if (flags.toslink) {
        uint8_t park_gpio = s_has_locked_gpio ? s_locked_gpio : RELAY_GPIO_VINYL;
        hw_set_relay_gpio(park_gpio);

        hw_set_dsp_mux2(INPUT_SRC_OPTICAL);
        s_state            = AS_STATE_PLAY_DIG;
        s_step_timer       = 0;
        s_total_scan_timer = 0;
        s_loss_hold_timer  = 0;
    }
    // ------------------------------------------------------------------------
    // ПРІОРИТЕТ №4: АНАЛОГ GPIO9 = 1 (LINE IN)
    // ------------------------------------------------------------------------
    else if (line_in_sig) {
        update_mux1_from_adc(flags.line_l, flags.line_r);
        hw_set_dsp_mux2(INPUT_SRC_ANALOG);

        s_locked_gpio      = RELAY_GPIO_LINE_IN;
        s_has_locked_gpio  = true;

        s_state            = AS_STATE_PLAY_ADC;
        s_step_timer       = 0;
        s_total_scan_timer = 0;
        s_loss_hold_timer  = 0;
    }
    // ------------------------------------------------------------------------
    // ЯКЩО ВСЮДИ ТИША:
    // ------------------------------------------------------------------------
    else if (s_state == AS_STATE_PLAY_ADC || s_state == AS_STATE_PLAY_DIG) {
        // Утримуємо поточний активний канал під час паузи між треками
        s_loss_hold_timer++;
        // ESP_LOGI(TAG, "Signal pause hold: %u/%u sec (keeping current channel)",
        //          (unsigned)s_loss_hold_timer, (unsigned)SIGNAL_LOSS_HOLD_SEC);

        if (s_loss_hold_timer >= SIGNAL_LOSS_HOLD_SEC) {
            // ESP_LOGI(TAG, "Hold time expired -> starting 60s scan");
            s_loss_hold_timer  = 0;
            s_state            = AS_STATE_SCAN;
            s_step_timer       = 0;
            s_total_scan_timer = 0;
        }
    }
    else if (s_state == AS_STATE_SCAN) {
        s_step_timer++;
        s_total_scan_timer++;

        if (s_total_scan_timer >= SCAN_TOTAL_TIME_SEC) {
            uint8_t park_gpio = s_has_locked_gpio ? s_locked_gpio : RELAY_GPIO_VINYL;
            hw_set_relay_gpio(park_gpio);
            s_state = AS_STATE_IDLE;
            // ESP_LOGI(TAG, "60s scan finished. Relay parked at GPIO9=%d", park_gpio);
        } else if (s_step_timer >= SCAN_STEP_WAIT_SEC) {
            s_step_timer = 0;
            hw_set_relay_gpio(s_relay_gpio == 0 ? 1 : 0);
        }
    } else {
        uint8_t hold_gpio = s_has_locked_gpio ? s_locked_gpio : RELAY_GPIO_VINYL;
        hw_set_relay_gpio(hold_gpio);
        s_state = AS_STATE_IDLE;
    }

    // Виводимо в термінал та на дисплей (на 2 сек) ЛИШЕ активний канал при зміні
    if (s_state == AS_STATE_PLAY_ADC || s_state == AS_STATE_PLAY_DIG) {
        const char *active_ch = autosearch_get_display_text();
        if (active_ch != s_last_logged_channel) {
            s_last_logged_channel = active_ch;
            s_popup_text          = active_ch;
            s_popup_until_ms      = now_ms + ACTIVE_CH_POPUP_MS;
            ESP_LOGI(TAG, "Active channel: %s", active_ch);
        }
    } else {
        s_last_logged_channel = NULL;
    }

    // 2. Щосекунди оновлюємо єдиний лічильник вінілу
    phono_timer_tick_1s();
}

// ============================================================================
// ГЕТТЕРИ СТАНУ
// ============================================================================

bool autosearch_is_scanning(void) {
    return (s_state == AS_STATE_SCAN);
}

const char* autosearch_get_display_text(void) {
    if (s_state == AS_STATE_SCAN) {
        return "SCAN";
    }

    switch (s_input_src) {
        case INPUT_SRC_ANALOG:
            return (s_relay_gpio == RELAY_GPIO_VINYL) ? "VINYL" : "LINE IN";
        case INPUT_SRC_BLE:
            return "BLUETOOTH";
        case INPUT_SRC_OPTICAL:
            return "OPTICAL";
        default:
            return "WAITING";
    }
}

const char* autosearch_get_active_popup_text(void) {
    if (s_popup_text == NULL) {
        return NULL;
    }
    int64_t now_ms = esp_timer_get_time() / 1000;
    if (now_ms >= s_popup_until_ms) {
        s_popup_text = NULL;
        return NULL;
    }
    return s_popup_text;
}

bool autosearch_is_phono_playing(void) {
    return (s_state == AS_STATE_PLAY_ADC &&
            s_relay_gpio == RELAY_GPIO_VINYL &&
            s_adc_signal_now);
}

autosearch_state_t autosearch_get_state(void) {
    return s_state;
}

audio_input_src_t autosearch_get_active_source(void) {
    return s_input_src;
}

uint8_t autosearch_get_relay_gpio(void) {
    return s_relay_gpio;
}
