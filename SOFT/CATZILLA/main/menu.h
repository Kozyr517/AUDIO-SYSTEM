#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include <stdbool.h>

// --- Константи номерів меню ---
#define MAIN_MENU_NUM       0
#define FILTERS_MENU_NUM    1
#define EQ_MENU_NUM         2
#define MYSPACE_MENU_NUM    3
#define PHONO_MENU_NUM      4

// --- Глобальні вказівники меню ---
extern uint8_t menu_pointer;
extern uint8_t main_menu_pointer;
extern uint8_t filters_menu_pointer;
extern uint8_t eq_menu_pointer;
extern uint8_t myspace_menu_pointer;
extern uint8_t phono_menu_pointer;

extern uint8_t old_menu_pointer;
extern uint8_t old_main_menu_pointer;
extern uint8_t old_filters_menu_pointer;
extern uint8_t old_eq_menu_pointer;
extern uint8_t old_myspace_menu_pointer;
extern uint8_t old_phono_menu_pointer;

// --- Збережені налаштування (RAM) ---
extern uint8_t saved_eq_preset;
extern uint8_t saved_filter;
extern uint8_t saved_myspace;
extern uint8_t saved_spatial;
extern uint8_t saved_sys;

// --- Масиви назв (потрібні для логування в buttons.c) ---
extern const char* eq_names[6];
extern const char* filter_names[6];
extern const char* myspace_names[7];
extern const char* spatial_names[2];
extern const char* sys_names[2];

// --- Функції меню ---
void menu_update(void);
//void eeprom_save_all_settings(void);
//void eeprom_load_all_settings(void);

#endif // MENU_H