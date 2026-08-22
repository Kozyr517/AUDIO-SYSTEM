#ifndef UI_APP_H
#define UI_APP_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    UI_STATE_BOOT,     // Стартовий стан: анімація + очікування стабілізації
    UI_STATE_ANALYZER, // Робочий режим: спектрограма / анімація тиші (кот)
    UI_STATE_MENU      // Робочий режим: меню
} ui_state_t;

void ui_app_init(void);
void ui_set_state(ui_state_t new_state);
ui_state_t ui_get_state(void);
uint8_t ui_get_volume(void);

#endif // UI_APP_H