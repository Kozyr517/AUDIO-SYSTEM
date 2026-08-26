#ifndef BUTTONS_H
#define BUTTONS_H

#include "driver/gpio.h"

// Піни кнопок
#define PIN_BTN_1 GPIO_NUM_1   
#define PIN_BTN_2 GPIO_NUM_2   
#define PIN_BTN_3 GPIO_NUM_41  
#define PIN_BTN_4 GPIO_NUM_42  

// Налаштування підсвітки кнопок
#define PIN_ADDR_L_FRONT GPIO_NUM_12
#define LED_COUNT_FRONT  4

void buttons_init(void);

#endif // BUTTONS_H