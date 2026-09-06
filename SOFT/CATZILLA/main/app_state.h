#ifndef APP_STATE_H
#define APP_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"

typedef enum {
    STATE_BOOT,
    STATE_IDLE_CAT2,
    STATE_SPECTRUM,
    STATE_VOLUME_POPUP,
    STATE_SETUP_MENU,
    STATE_SLEEP_SHUTDOWN
} app_state_t;

typedef struct {
    const char *name;
    uint8_t addr[8];
    float temp;
    bool valid;
} sensor_t;

extern volatile app_state_t current_state;
extern uint8_t master_volume;
extern TickType_t last_vol_activity_tick;
extern sensor_t g_sensors[];
extern const size_t g_sensors_count;

#endif // APP_STATE_H