#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "audio_manager.h"

// Розводка реле (GPIO 9): 0 = Вініл (Фонокоректор), 1 = Лінійний вхід (Line In)
#define RELAY_GPIO_PIN            GPIO_NUM_9
#define RELAY_GPIO_VINYL          0
#define RELAY_GPIO_LINE_IN        1

// Стани автопошуку
typedef enum {
    AS_STATE_SCAN = 0,       // 60-секундний активний пошук реле (3с GPIO=0 <-> 3с GPIO=1)
    AS_STATE_PLAY_ADC,       // Грає аналог (VINYL або LINE IN), реле зафіксоване
    AS_STATE_PLAY_DIG,       // Грає цифра (BLUETOOTH або OPTICAL), реле чекає на своєму місці
    AS_STATE_IDLE            // Повна тиша (після 60с пошуку або в паузі), реле не клацає
} autosearch_state_t;

// ==================== ОСНОВНІ ФУНКЦІЇ АВТОПОШУКУ ====================

void autosearch_init(void);
void autosearch_tick_1s(void);
void autosearch_force_scan_from_button(void);
bool autosearch_is_scanning(void);
const char* autosearch_get_display_text(void);
const char* autosearch_get_active_popup_text(void);

autosearch_state_t autosearch_get_state(void);
audio_input_src_t  autosearch_get_active_source(void);
uint8_t            autosearch_get_relay_gpio(void);

// ==================== ФУНКЦІЇ ЛІЧИЛЬНИКА ГОЛКИ ТА ВІНІЛУ (PHONO MM) ====================

bool autosearch_is_phono_playing(void);
void phono_get_needle_time_str(char *buf, uint8_t buf_size);
void phono_get_total_time_str(char *buf, uint8_t buf_size);
void phono_reset_needle_time(void);
void phono_reset_total_time(void);
void phono_save_now(void);
