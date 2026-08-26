#include "anim_cat1.h"
#include "gp1247ai.h"

#define CAT1_W       48
#define CAT1_H       48
#define CAT1_FRAMES  4

extern TypeDef_GP1247AI lcd;

static const uint8_t frame_0[CAT1_W * CAT1_H / 8] = {0};
static const uint8_t frame_1[CAT1_W * CAT1_H / 8] = {0};

static const uint8_t* const cat1_frames[CAT1_FRAMES] = {
    frame_0, frame_1, frame_0, frame_1
};

void anim_cat1_draw(uint16_t x, uint16_t y) {
    static uint8_t current_frame = 0;
    static uint8_t frame_divider = 0;

    LCD_DrawBitmap(&lcd, x, y, cat1_frames[current_frame], CAT1_W, CAT1_H, 1);

    // Уповільнення анімації (GIF-ефект): перемикаємо кадр кожні 2 виклики
    if (++frame_divider >= 2) {
        frame_divider = 0;
        current_frame = (current_frame + 1) % CAT1_FRAMES;
    }
}