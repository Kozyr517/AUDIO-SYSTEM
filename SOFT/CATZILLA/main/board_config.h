#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "driver/gpio.h"

// =============================================================================
// I2C BUSES (Аудіо чипи та Дисплей)
// =============================================================================
// I2C_NUM_0: Аудіо периферія (3x AK4493, 1x AK5572, ADAU1452)
#define AUDIO_I2C_PORT          I2C_NUM_0
#define PIN_AUDIO_I2C_SDA       GPIO_NUM_7
#define PIN_AUDIO_I2C_SCL       GPIO_NUM_15

// I2C_NUM_1: Дисплей (VFD)
#define VFD_I2C_PORT            I2C_NUM_1
#define PIN_VFD_I2C_SDA         GPIO_NUM_16
#define PIN_VFD_I2C_SCL         GPIO_NUM_17

// =============================================================================
// TOUCH BUTTONS (TTP223 + Interrupts)
// =============================================================================
#define PIN_TOUCH_BT1           GPIO_NUM_4
#define PIN_TOUCH_BT2           GPIO_NUM_5
#define PIN_TOUCH_BT3           GPIO_NUM_6
#define PIN_TOUCH_BT4           GPIO_NUM_21

// =============================================================================
// BACKLIGHT LEI (WS2812B / Addressable LEDs)
// =============================================================================
#define PIN_LEDS_DATA           GPIO_NUM_18
#define LED_STRIP_NUM_LEDS      4

// =============================================================================
// THERMAL MANAGEMENT (DS18B20 1-Wire & PWM Fans)
// =============================================================================
#define PIN_DS18B20_1_WIRE      GPIO_NUM_1

#define PIN_FAN1_PWM            GPIO_NUM_2
#define PIN_FAN2_PWM            GPIO_NUM_42

// =============================================================================
// POWER CONTROL & MUTE RELAYS
// =============================================================================
#define PIN_PWR_MAIN_RELAY      GPIO_NUM_38
#define PIN_PWR_AMP_STBY        GPIO_NUM_39
#define PIN_AUDIO_MUTE          GPIO_NUM_40

// =============================================================================
// SYSTEM & DIAGNOSTICS
// =============================================================================
#define PIN_SYS_LED             GPIO_NUM_41

#endif // BOARD_CONFIG_H