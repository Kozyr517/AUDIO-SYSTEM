#ifndef __SCREENS_H__
#define __SCREENS_H__

#include <stdint.h>
#include <stdbool.h>

// Базові екрани та анімації
void draw_boot_animation(void);
void draw_idle_cat2_frame(void);
void draw_mute_frame(void);

// Аналізатор спектра (падаючі крапочки)
void draw_spectrum_analyzer_frame(void);

// Спливаюче вікно гучності
void draw_volume_popup(uint8_t master_volume);

#endif // __SCREENS_H__