#ifndef _ANALIZATOR
#define _ANALIZATOR

#include "driver/i2s_std.h"
#include "esp_dsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <math.h>

#define RX_SIZE 1024
#define COLUM_SIZE 254

#define I2S_DIN GPIO_NUM_18
#define I2S_BCK GPIO_NUM_17
#define I2S_LRCK GPIO_NUM_16

// Додано глобальний експорт прапорця для main.c
extern volatile uint8_t is_input_sig_flag;

void analizator_init(void);
void clear_byffer(void);

#endif // _ANALIZATOR