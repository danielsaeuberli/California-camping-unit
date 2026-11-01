/* Touch interface (SPEC 3.2): controller per Viewe datasheet on I2C 1, INT wakes from
 * light sleep. The first touch after waking only wakes (buzzer), no operation (SPEC 9). */
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct touch touch_t;
struct touch {
    int (*init)(touch_t *t);
    bool (*read)(touch_t *t, int32_t *x, int32_t *y); /* true while pressed */
    void *ctx;
};
