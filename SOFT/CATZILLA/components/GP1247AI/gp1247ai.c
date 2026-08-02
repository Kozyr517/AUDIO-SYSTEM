/*
 * gp1247ai.c
 *
 *  Created on: Nov 12, 2024
 *      Author: Jackson
 */

#include "gp1247ai.h"

uint8_t v_buffer[2028];

void GP1247AI_Ini(TypeDef_GP1247AI *p_lcd_obj) {
	uint8_t lcd_reset = 0xAA;
	p_lcd_obj->dma_spi_transmit(&lcd_reset, 1);
	p_lcd_obj->delay_ms(1);
	uint8_t lcd_osc[] = {0x78, 0x08};
	p_lcd_obj->dma_spi_transmit(lcd_osc, 2);
	p_lcd_obj->delay_ms(1);
	uint8_t vfd_mode_settings[] = {0xCC, 0x05, 0x00};
	p_lcd_obj->dma_spi_transmit(vfd_mode_settings, 3);
	p_lcd_obj->delay_ms(1);
	uint8_t display_area_settings[] = {0xE0, 0xFC, 0x3E, 0x00, 0x20, 0x80, 0x80, 0x80};
	p_lcd_obj->dma_spi_transmit(display_area_settings, 8);
	p_lcd_obj->delay_ms(1);
	uint8_t internal_speed[] = {0xB1, 0x20, 0x3F, 0x00, 0x01};
	p_lcd_obj->dma_spi_transmit(internal_speed, 5);
	p_lcd_obj->delay_ms(1);
	uint8_t dimming_level_settings[] = {0xA0, 0x01, 0x00};
	p_lcd_obj->dma_spi_transmit(dimming_level_settings, 3);
	p_lcd_obj->delay_ms(1);
	uint8_t lcd_clear_gram = 0x55;
	p_lcd_obj->dma_spi_transmit(&lcd_clear_gram, 1);
	p_lcd_obj->delay_ms(20);
	uint8_t display_position_offset[] = {0xC0, 0x00, 0x00};
	p_lcd_obj->dma_spi_transmit(display_position_offset, 3);
	p_lcd_obj->delay_ms(1);
	uint8_t display_mode[] = {0x80, 0x00};
	p_lcd_obj->dma_spi_transmit(display_mode, 2);
	p_lcd_obj->delay_ms(1);

	p_lcd_obj->video_buffer = v_buffer + 4;
}

void LCD_clear(TypeDef_GP1247AI *p_lcd_obj) {
	for (uint32_t i = 0; i < (PIX_SIZE_X * PIX_SIZE_Y) / 8; i++) {
		p_lcd_obj->video_buffer[i] = 0x00;
	}
}

void LCD_Update(TypeDef_GP1247AI *p_lcd_obj) {
	v_buffer[0] = 0xF0;
	v_buffer[1] = 0x00;
	v_buffer[2] = 0x00;
	v_buffer[3] = 0x3F;
	p_lcd_obj->dma_spi_transmit(p_lcd_obj->video_buffer - 4, 2028);
}

void LCD_DrawPixel(TypeDef_GP1247AI *p_lcd_obj, uint8_t x, uint8_t y, uint8_t color) {
	if ((x < 0) || (x >= PIX_SIZE_X) || (y < 0) || (y >= PIX_SIZE_Y))
		return;

	if (color)
		p_lcd_obj->video_buffer[x * 8 + (y / 8)] |= 1 << (y % 8);
	else
		p_lcd_obj->video_buffer[x * 8 + (y / 8)] &= ~(1 << (y % 8));
}

void LCD_DrawChar(TypeDef_GP1247AI *p_lcd_obj, const char c, uint8_t x, uint8_t y, const uint8_t *p_font, uint8_t inversion) {
	const uint16_t max_count = p_font[0] * p_font[1];
	uint16_t arr_pos = (c - p_font[2]) * (p_font[0] * p_font[1] / 8) + 4;
	uint8_t b = 0x80;
	uint8_t x_pos = 0;
	uint8_t y_pos = 0;
	uint16_t i = 0;

	for (; i < max_count; i++) {
		if (p_font[arr_pos] & b) {
			LCD_DrawPixel(p_lcd_obj, x + x_pos, y + y_pos, !inversion);
		}
		if (b & 0x01) {
			b = 0x80;
			arr_pos++;
		} else {
			b = b >> 1;
		}
		if (x_pos < (p_font[0] - 1)) {
			x_pos++;
		} else {
			x_pos = 0;
			y_pos++;
		}
	}
}

void LCD_print(TypeDef_GP1247AI *p_lcd_obj, const char *str, uint8_t x, uint8_t y, const uint8_t *p_font, uint8_t inversion) {
	while (*str) {
		LCD_DrawChar(p_lcd_obj, *str++, x, y, p_font, inversion);
		x = x + p_font[0];
	}
}

// Рисование линии
void LCD_DrawLine(TypeDef_GP1247AI *p_lcd_obj, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color) {
	int steep = abs(y1 - y0) > abs(x1 - x0);
	if (steep) {
		swap(x0, y0);
		swap(x1, y1);
	}
	if (x0 > x1) {
		swap(x0, x1);
		swap(y0, y1);
	}
	int dx, dy;
	dx = x1 - x0;
	dy = abs(y1 - y0);
	int err = dx / 2;
	int ystep;
	if (y0 < y1) {
		ystep = 1;
	} else {
		ystep = -1;
	};
	for (; x0 <= x1; x0++) {
		if (steep) {
			LCD_DrawPixel(p_lcd_obj, y0, x0, color);
		} else {
			LCD_DrawPixel(p_lcd_obj, x0, y0, color);
		};
		err -= dy;
		if (err < 0) {
			y0 += ystep;
			err += dx;
		}
	}
}

void LCD_DrawFastVLine(TypeDef_GP1247AI *p_lcd_obj, uint8_t x, uint8_t y, uint8_t h, uint8_t color) {

	if ((x < PIX_SIZE_X) && (y < PIX_SIZE_Y)) {
		for (int cy = 0; cy < h; cy++) {
			LCD_DrawPixel(p_lcd_obj, x, y + cy, color);
		}
	}
}

void LCD_DrawFastHLine(TypeDef_GP1247AI *p_lcd_obj, uint8_t x, uint8_t y, uint8_t w, uint8_t color) {
	LCD_DrawLine(p_lcd_obj, x, y, x + w - 1, y, color);
}

void LCD_DrawRect(TypeDef_GP1247AI *p_lcd_obj, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color) {
	LCD_DrawFastHLine(p_lcd_obj, x, y, w, color);
	LCD_DrawFastHLine(p_lcd_obj, x, y + h - 1, w, color);
	LCD_DrawFastVLine(p_lcd_obj, x, y, h, color);
	LCD_DrawFastVLine(p_lcd_obj, x + w - 1, y, h, color);
}

void LCD_FillRect(TypeDef_GP1247AI *p_lcd_obj, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color) {
	for (int16_t i = x; i < x + w; i++) {
		LCD_DrawFastVLine(p_lcd_obj, i, y, h, color);
	}
}

void LCD_DrawBitmap(TypeDef_GP1247AI *p_lcd_obj, uint8_t x, uint8_t y, const uint8_t *bitmap, uint8_t w, uint8_t h, uint8_t color) {
	for (int16_t j = 0; j < h; j++) {
		for (int16_t i = 0; i < w; i++) {
			if (bitmap[i + (j / 8) * w] & 1 << (j % 8)) {
				LCD_DrawPixel(p_lcd_obj, x + i, y + j, color);
			}
		}
	}
}
