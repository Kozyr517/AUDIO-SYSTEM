#ifndef _ANALIZATOR_H
#define _ANALIZATOR_H

#include "driver/i2s_std.h"
#include "esp_dsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <math.h>

#define RX_SIZE 1024
#define COLUM_SIZE 254
// ==================== ПІНИ RX ====================
#define I2S_DIN  GPIO_NUM_18
#define I2S_BCK  GPIO_NUM_17
#define I2S_LRCK GPIO_NUM_16

void analizator_init(void);
extern uint8_t is_input_sig_flag;

#endif // _ANALIZATOR_H