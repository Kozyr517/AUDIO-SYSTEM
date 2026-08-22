#include "anim_cat1.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gp1247ai.h"
#include "Font16x16.h"

static const char *TAG = "ANIM_CAT1";

extern TypeDef_GP1247AI lcd; // Об'єкт оголошено в lcd.c

void anim_cat1_draw(void) {
    const uint16_t SCREEN_WIDTH = 256; // Виправлено на uint16_t для 256 пікселів
    const uint8_t SCREEN_HEIGHT = 64;
    const uint8_t cat_y = (SCREEN_HEIGHT - CAT1_H) / 2;

    static int16_t x = -CAT1_W;
    static uint8_t current_frame = 0;
    static uint8_t state = 0; // 0 - рух до центру, 1 - фінальний кадр з текстом

    const uint8_t step_x = 4;
    int target_x = (SCREEN_WIDTH - CAT1_W) / 2;

    if (state == 0) {
        // Етап руху кота
        LCD_clear(&lcd);
        LCD_DrawBitmap(&lcd, (uint8_t)x, cat_y, cat1_frames[current_frame], CAT1_W, CAT1_H, 1);
        LCD_Update(&lcd);

        current_frame = (current_frame + 1) % CAT1_FRAMES_COUNT;
        x += step_x;

        if (x > target_x) {
            x = target_x;
            state = 1; // Переходимо до фінального стану
        }
    } 
    else if (state == 1) {
        // Фінальний кадр з текстом "CATZILLA"
        LCD_clear(&lcd);
        LCD_DrawBitmap(&lcd, (SCREEN_WIDTH - CAT1_W) / 2, cat_y - 8, cat1_frames[0], CAT1_W, CAT1_H, 1);
        
        // Виправлено назву на LCD_print та додано &lcd (за аналогією з іншими функціями дисплея)
        LCD_print(&lcd, "CATZILLA", (SCREEN_WIDTH - (8 * 16)) / 2, cat_y + 12, (const uint8_t*)Font16x16, 1);
        
        LCD_Update(&lcd);
    }
}