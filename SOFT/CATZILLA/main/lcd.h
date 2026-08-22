#ifndef LCD_H
#define LCD_H

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gp1247ai.h"

#define PIN_NUM_SCLK   GPIO_NUM_39
#define PIN_NUM_MOSI   GPIO_NUM_40
#define PIN_NUM_LCD_CS GPIO_NUM_48
#define PIN_NUM_LCD_RS GPIO_NUM_38

void lcd_init(void);
void lcd_bus_init(void);
void lcd_draw_colum(uint8_t pos, uint8_t val);
void lcd_set_dot(uint8_t pos, uint8_t height);
void lcd_clear(void);
void lcd_update(void);
void lcd_clear_and_update(void);
void lcd_print(const char *str, uint8_t x, uint8_t y, const uint8_t *p_font, uint8_t inversion);
void lcd_draw_rectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
void lcd_set_brightness(uint16_t brightness);

#endif // LCD_H