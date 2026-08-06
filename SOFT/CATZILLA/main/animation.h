#ifndef ANIMATION_H
#define ANIMATION_H

#include <stdint.h>

// Розміри кадру анімації (як прописано у вашому menu.c)
#define CAT_ANIM_W 64
#define CAT_ANIM_H 48

// Кількість кадрів
#define CAT_BOOT_FRAMES 2

// ==========================================
// ТЕСТОВІ КАДРИ (64 * 48 / 8 = 384 байти кожен)
// Потім замініть ці масиви на нормальну графіку
// ==========================================

// Кадр 1: Смуги або частковий шум (решта масиву автоматично заповниться нулями)
static const uint8_t frame_1[384] = {
    0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 
    0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55
};

// Кадр 2: Інвертовані смуги для ефекту руху
static const uint8_t frame_2[384] = {
    0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 
    0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA
};

// Головний масив анімації, до якого звертається цикл у menu.c
static const uint8_t *boot_cat_anim[CAT_BOOT_FRAMES] = {
    frame_1,
    frame_2
};

#endif // ANIMATION_H