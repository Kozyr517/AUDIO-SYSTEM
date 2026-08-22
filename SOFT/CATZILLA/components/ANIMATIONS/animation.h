#ifndef ANIMATION_H
#define ANIMATION_H

#include <stdint.h>

// Ідентифікатори для пунктів меню
typedef enum {
    ANIM_CAT1 = 0, // Кіт і метелик
    ANIM_CAT2,     // Кіт і м'ячик
    ANIM_CAT3,     // Майбутня анімація 3
    ANIM_CAT4,     // Майбутня анімація 4
    ANIM_CAT5,     // Майбутня анімація 5
    ANIM_CAT6,     // Майбутня анімація 6
    ANIM_CAT7,     // Майбутня анімація 7
    ANIM_COUNT
} anim_id_t;

// Викликається під час вибору анімації в меню
void animation_select(anim_id_t id);

// Отримати ID поточної анімації (для меню)
anim_id_t animation_get_current(void);

// Головна функція малювання, яку викликає ui_app.c
void animation_draw(void);

#endif // ANIMATION_H