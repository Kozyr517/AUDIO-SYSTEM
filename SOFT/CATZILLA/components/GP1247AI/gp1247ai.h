/*
 * gp1247ai.h
 *
 *  Created on: Nov 12, 2024
 *      Author: Jackson
 */

#ifndef GP1247AI_GP1247AI_H_
#define GP1247AI_GP1247AI_H_

#include <stdlib.h>
#include <esp_log.h>

#define PIX_SIZE_X 253
#define PIX_SIZE_Y 64
#define LINE_SIZE_Y 8

#define swap(a, b) {uint8_t t = a; a = b; b = t; }


typedef struct {
	void (*dma_spi_transmit)(uint8_t *data, size_t size);
	void (*delay_ms)(uint32_t delay);
	uint8_t *video_buffer;
}TypeDef_GP1247AI;


void GP1247AI_Ini(TypeDef_GP1247AI *p_lcd_obj);
void LCD_Update(TypeDef_GP1247AI *p_lcd_obj);
void LCD_clear(TypeDef_GP1247AI *p_lcd_obj);
void LCD_print(TypeDef_GP1247AI *p_lcd_obj, const char *str, uint8_t x, uint8_t y, const uint8_t *p_font, uint8_t inversion);
void LCD_DrawPixel(TypeDef_GP1247AI *p_lcd_obj, uint8_t x, uint8_t y, uint8_t color);
void LCD_DrawLine(TypeDef_GP1247AI *p_lcd_obj, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color);
void LCD_DrawFastVLine(TypeDef_GP1247AI *p_lcd_obj, uint8_t x, uint8_t y, uint8_t h, uint8_t color);
void LCD_DrawFastHLine(TypeDef_GP1247AI *p_lcd_obj, uint8_t x, uint8_t y, uint8_t w, uint8_t color);
void LCD_DrawRect(TypeDef_GP1247AI *p_lcd_obj, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);
void LCD_FillRect(TypeDef_GP1247AI *p_lcd_obj, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);
void LCD_DrawBitmap(TypeDef_GP1247AI *p_lcd_obj, uint8_t x, uint8_t y, const uint8_t *bitmap, uint8_t w, uint8_t h, uint8_t color);

#endif /* GP1247AI_GP1247AI_H_ */
