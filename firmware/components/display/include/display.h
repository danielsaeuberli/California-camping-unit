/* Display interface (SPEC 3.2). Implementations: display_viewe68 (MIPI DSI, M3),
 * fallback display_nhd68 (Newhaven, HARDWARE Annex A), display_sdl on the host.
 * Open: Viewe order number, 2 lanes, PWM backlight, init sequence (O15). */
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct display display_t;
struct display {
    int (*init)(display_t *d);
    int (*on)(display_t *d);
    int (*off)(display_t *d);
    int (*set_brightness)(display_t *d, uint8_t pct); /* on/off only until PWM is confirmed (O15) */
    void *ctx;
};
