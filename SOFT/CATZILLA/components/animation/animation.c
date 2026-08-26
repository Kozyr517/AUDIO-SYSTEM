#include "animation.h"
#include <stddef.h>

#include "anim_cat1.h"
#include "anim_cat2.h"
#include "anim_cat3.h"
#include "anim_cat4.h"
#include "anim_cat5.h"
#include "anim_cat6.h"
#include "anim_cat7.h"

typedef void (*anim_draw_func_t)(uint16_t x, uint16_t y);

static const anim_draw_func_t anim_registry[ANIM_COUNT] = {
    anim_cat1_draw,
    anim_cat2_draw,
    anim_cat3_draw,
    anim_cat4_draw,
    anim_cat5_draw,
    anim_cat6_draw,
    anim_cat7_draw,
};

void animation_draw(anim_id_t id, uint16_t x, uint16_t y) {
    if (id < ANIM_COUNT && anim_registry[id] != NULL) {
        anim_registry[id](x, y);
    }
}