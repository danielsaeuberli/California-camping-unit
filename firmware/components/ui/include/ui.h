/* LVGL user interface (SPEC 9), after the current web UI in ui/reference/web_v1/.
 * Reads state_bus snapshots, sends commands through the queue; never touches outputs. */
#pragma once

#include "lvgl.h"
#include "modules.h"
#include "state_bus.h"

typedef enum { UI_PAGE_OVERVIEW = 0, UI_PAGE_ROOF, UI_PAGE_FRIDGE, UI_PAGE_ENERGY, UI_PAGE_HEAT, UI_PAGE_POWER,
               UI_PAGE_WATER, UI_PAGE_SETTINGS, UI_PAGE_COUNT } ui_page_t;

typedef struct {
    state_bus_t *bus;
    bool simulation;       /* shows the "Simulation" badge */
    uint8_t return_min;    /* back to the overview after this many minutes without input */
} ui_config_t;

/* Builds the 1280 x 480 interface inside parent (the screen on the device). */
void ui_init(lv_obj_t *parent, const ui_config_t *cfg);
void ui_show(ui_page_t page);
/* Call for every press on the touch panel (platform input hook): any touch stops a
 * tap-started roof run (SPEC 7.1). x/y in screen coordinates. */
void ui_touch_pressed(int32_t x, int32_t y);
lv_obj_t *ui_root(void);
/* Same as tapping Öffnen/Schliessen on the roof page (simulator, tests). */
void ui_roof_tap(roof_dir_t dir);
