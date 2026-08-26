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

void menu_update(void);

#endif /* MENU_H_ */