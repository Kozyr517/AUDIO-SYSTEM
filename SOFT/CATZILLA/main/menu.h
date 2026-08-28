#ifndef MENU_H_
#define MENU_H_

#include <stdint.h>
#include "lcd.h" 

extern TypeDef_GP1247AI lcd;

#define MAIN_MENU_NUM    0
#define FILTERS_MENU_NUM 1
#define EQ_MENU_NUM      2
#define BALANCE_MENU_NUM 3
#define PHONO_MENU_NUM   6

// Вказівники позицій курсора у меню
extern uint8_t menu_pointer;
extern uint8_t main_menu_pointer;
extern uint8_t filters_menu_pointer;
extern uint8_t eq_menu_pointer;
extern uint8_t balance_menu_pointer;
extern uint8_t phono_menu_pointer;

extern uint8_t old_menu_pointer;
extern uint8_t old_main_menu_pointer;
extern uint8_t old_filters_menu_pointer;
extern uint8_t old_eq_menu_pointer;
extern uint8_t old_balance_menu_pointer;
extern uint8_t old_phono_menu_pointer;

// ==================== ЗБЕРЕЖЕНІ НАЛАШТУВАННЯ В RAM ====================
extern uint8_t saved_eq_preset;
extern uint8_t saved_filter;
extern uint8_t saved_balance;
extern uint8_t saved_spatial_3d; // 0 = OFF, 1 = ON
extern uint8_t saved_night_mode; // 0 = OFF, 1 = ON

// Текстові назви для виводу в термінал
extern const char* eq_names[6];
extern const char* filter_names[6];
extern const char* balance_names[3];

// ==================== РОБОТА З EEPROM ====================
void eeprom_save_all_settings(void);
void eeprom_load_all_settings(void);

void menu_update(void);

#endif /* MENU_H_ */