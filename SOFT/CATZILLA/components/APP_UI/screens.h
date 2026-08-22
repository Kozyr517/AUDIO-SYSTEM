#ifndef __SCREENS_H__
#define __SCREENS_H__

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "gp1247ai.h" 

void show_startup_animation(void);
void show_spectr(void);
void show_param_menu(void);
void show_catzilla_screen(uint8_t volume);

#endif // __SCREENS_H__