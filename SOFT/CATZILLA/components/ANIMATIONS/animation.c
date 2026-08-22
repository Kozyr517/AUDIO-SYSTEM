#include "animation.h"
#include <stddef.h>

// Підключаємо заголовні файли кожної анімації без зайвих підкреслень
#include "anim_cat1.h"
#include "anim_cat2.h"
#include "anim_cat3.h"
#include "anim_cat4.h"
#include "anim_cat5.h"
#include "anim_cat6.h"
#include "anim_cat7.h"

typedef void (*anim_draw_func_t)(void);

typedef struct {
    anim_id_t id;
    const char *name;
    anim_draw_func_t draw_func;
} anim_entry_t;

// ГОЛОВНИЙ МАСИВ АНІМАЦІЙ
static const anim_entry_t anim_registry[ANIM_COUNT] = {
    { ANIM_CAT1, "Cat 1",          anim_cat1_draw },
    { ANIM_CAT2, "Cat 2",          anim_cat2_draw },
    { ANIM_CAT3, "Cat 3",          anim_cat3_draw },
    { ANIM_CAT4, "Cat 4",          anim_cat4_draw },
    { ANIM_CAT5, "Cat 5",          anim_cat5_draw },
    { ANIM_CAT6, "Cat 6",          anim_cat6_draw },
    { ANIM_CAT7, "Cat 7",          anim_cat7_draw },
};

static anim_id_t current_anim_id = ANIM_CAT1;

void animation_select(anim_id_t id) {
    if (id < ANIM_COUNT) {
        current_anim_id = id;
    }
}

anim_id_t animation_get_current(void) {
    return current_anim_id;
}

void animation_draw(void) {
    if (anim_registry[current_anim_id].draw_func != NULL) {
        anim_registry[current_anim_id].draw_func();
    }
}