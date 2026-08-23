#include "screens.h"
#include "analizator.h"
#include "GP1247AI.h"
#include "Font16x16.h"
#include "Sinclair_S8x8.h"

// Зовнішня змінна дисплея, ініціалізована в основному модулі
extern TypeDef_GP1247AI lcd;

#define VOL_MAX  100
#define NUM_PAWS 6

static const uint8_t paw_filled_bmp[20] = {
    0x00, 0x00, 0x06, 0x60, 0x0F, 0xF0, 0x0F, 0xF0,
    0x1F, 0xF8, 0x3F, 0xFC, 0x7F, 0xFE, 0x7F, 0xFE,
    0x3F, 0xFC, 0x0F, 0xF0
};

static const uint8_t paw_empty_bmp[20] = {
    0x00, 0x00, 0x06, 0x60, 0x09, 0x90, 0x09, 0x90,
    0x10, 0x08, 0x20, 0x04, 0x40, 0x02, 0x40, 0x02,
    0x20, 0x04, 0x0F, 0xF0
};

// Стартова анімація для процесу стабілізації
void show_startup_animation(void) {
    LCD_clear(&lcd);
    LCD_print(&lcd, "CATZILLA SYSTEM", 10, 10, Font16x16, 0);
    LCD_print(&lcd, "POWER STABILIZING...", 15, 32, Sinclair_S8x8, 0);
    LCD_Update(&lcd);
}

static void draw_cat_volume_bar(uint8_t volume) {
    uint8_t active_paws = (volume * NUM_PAWS + (VOL_MAX / 2)) / VOL_MAX;
    uint8_t start_x = 168;
    uint8_t start_y = 12;
    uint8_t paw_w = 10;
    uint8_t paw_gap = 3;

    uint8_t total_width = NUM_PAWS * (paw_w + paw_gap) + 3;
    LCD_DrawRect(&lcd, start_x - 3, start_y - 3, total_width, 16, 1);

    for (uint8_t i = 0; i < NUM_PAWS; i++) {
        uint8_t x_pos = start_x + (i * (paw_w + paw_gap));
        if (i < active_paws) {
            LCD_DrawBitmap(&lcd, x_pos, start_y, paw_filled_bmp, 10, 10, 1);
        } else {
            LCD_DrawBitmap(&lcd, x_pos, start_y, paw_empty_bmp, 10, 10, 1);
        }
    }
}

void show_catzilla_screen(uint8_t volume) {
    LCD_clear(&lcd);
    LCD_print(&lcd, "CATZILLA", 10, 10, Font16x16, 0);
    LCD_print(&lcd, "ONLINE", 20, 30, Font16x16, 0);

    draw_cat_volume_bar(volume);
    LCD_print(&lcd, "Зміна гучності", 155, 38, Sinclair_S8x8, 0);
    LCD_Update(&lcd);
}