#ifndef ANIMATION_H_
#define ANIMATION_H_

#include <stdint.h>

typedef enum {
    ANIM_CAT1 = 0,
    ANIM_CAT2,
    ANIM_CAT3,
    ANIM_CAT4,
    ANIM_CAT5,
    ANIM_CAT6,
    ANIM_CAT7,
    ANIM_COUNT
} anim_id_t;

void animation_draw(anim_id_t id, uint16_t x, uint16_t y);

#endif // ANIMATION_H_