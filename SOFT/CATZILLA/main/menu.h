#ifndef MENU_H_
#define MENU_H_

#include "gp1247ai.h"
#include "driver/gpio.h"
#include "led_strip.h"

// ==================== ПІНИ VFD ДИСПЛЕЯ ====================
#define PIN_NUM_MOSI    GPIO_NUM_40
#define PIN_NUM_SCLK    GPIO_NUM_39
#define PIN_NUM_LCD_CS  GPIO_NUM_48
#define PIN_NUM_LCD_RS  GPIO_NUM_38

// Оголошення функцій
void display_init(void);
void buttons_init(void);
void menu_update(void);
void leds_init(void);

#endif