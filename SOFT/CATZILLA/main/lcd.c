#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "lcd.h"

TypeDef_GP1247AI lcd;
spi_device_handle_t g_spi_handle;

void dma_spi_transmit(uint8_t *data, size_t size) {
	spi_transaction_t trans = {.tx_buffer = (void *)data, .length = (8 * size)};

	spi_device_transmit(g_spi_handle, &trans);
}

void lcd_bus_init(void) {
	spi_bus_config_t buscfg = {.sclk_io_num = PIN_NUM_SCLK,
							   .mosi_io_num = PIN_NUM_MOSI,
							   .miso_io_num = -1,
							   .quadwp_io_num = -1,
							   .quadhd_io_num = -1,
							   .max_transfer_sz = 2100};
	ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

	spi_device_interface_config_t dev_config = {
		.clock_source = SOC_MOD_CLK_XTAL,
		.clock_speed_hz = 2000000,
		.mode = 0,
		.spics_io_num = PIN_NUM_LCD_CS,
		.queue_size = 300,
		.post_cb = NULL,
		.pre_cb = NULL,
		.flags = SPI_DEVICE_TXBIT_LSBFIRST,
	};
	spi_bus_add_device(SPI2_HOST, &dev_config, &g_spi_handle);
	lcd.dma_spi_transmit = dma_spi_transmit;
	lcd.delay_ms = vTaskDelay;
}

void lcd_init(void) { GP1247AI_Ini(&lcd); }

void lcd_draw_colum(uint8_t pos, uint8_t val) {
	if (pos > 252) {
		pos = 252;
	}
	if (val > 63) {
		val = 63;
	}

	LCD_DrawFastVLine(&lcd, pos, 63 - val, val, 1);
}

void lcd_set_dot(uint8_t pos, uint8_t height) {
	if (pos > 252) {
		pos = 252;
	}
	if (height > 63) {
		height = 63;
	}
	LCD_DrawPixel(&lcd, pos, 63 - height, 1);
}

void lcd_clear(void) { LCD_clear(&lcd); }

void lcd_update(void) { LCD_Update(&lcd); }

void lcd_clear_and_update(void) {
	LCD_clear(&lcd);
	LCD_Update(&lcd);
}

void lcd_print(const char *str, uint8_t x, uint8_t y, const uint8_t *p_font, uint8_t inversion) {
	LCD_print(&lcd, str, x, y, p_font, inversion);
}

void lcd_draw_rectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h) { LCD_DrawRect(&lcd, x, y, w, h, 1); }

void lcd_set_brightness(uint16_t brightness) { GP1247AI_set_brightness(&lcd, brightness); }