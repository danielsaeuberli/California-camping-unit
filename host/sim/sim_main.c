/* Host simulator (SPEC 3.4): the device UI (1280 x 480) on top, a control panel below
 * for ignition, driving, DI and AI. Uses profiles/sim/sim-overrides.json (invented
 * simulation values, clearly marked in the UI) unless --profile real is given.
 *
 *   camping_sim [--profile real|sim] [--page overview|roof|fridge] [--shot file.bmp [--shot-all]]
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "control.h"
#include "hal_sim.h"
#include "lvgl.h"
#include "profile.h"
#include "state_bus.h"
#include "ui.h"

#define DEV_W 1280
#define DEV_H 480
#define PANEL_H 300

static profile_t P;
static hal_sim_t SIM;
static hal_io_t IO;
static state_bus_t BUS;
static control_t CTL;
static bool sim_profile = true;
static uint32_t fake_ms; /* clock in --shot mode */

/* Simulated world (sim only, not vehicle data). */
static struct {
    bool ignition, driving, key86s, cerbo, shore, waste_full;
    float roof;      /* 0 closed .. 1 open */
    float fridge_c;  /* cabinet temperature */
    float outside_c;
    int fresh_step;  /* 0..3 */
    int soc;
    uint32_t heater_on_ms, heater_off_ms;
    bool run;
} WRLD = {.ignition = true, .cerbo = true, .fridge_c = 9.0f, .outside_c = 12.5f, .fresh_step = 2, .soc = 76};

static lv_obj_t *di_btn[CU_N_DI];

static uint32_t tick_cb(void)
{
    return fake_ms;
}

static char *slurp(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *b = malloc((size_t)n + 1);
    if (fread(b, 1, (size_t)n, f) != (size_t)n) {
        n = 0;
    }
    b[n] = 0;
    fclose(f);
    return b;
}

/* Sim NTC from the two upper sim curve points (beta model, simulation only). */
static float ntc_volts(float temp_c)
{
    const float r25 = 10000.0f, r5 = 25400.0f;
    float b = logf(r5 / r25) / (1.0f / 278.15f - 1.0f / 298.15f);
    float r = r25 * expf(b * (1.0f / (temp_c + 273.15f) - 1.0f / 298.15f));
    return 3.3f * r / (r + 10000.0f);
}

static void set_di(uint8_t ch, bool on)
{
    if (on) {
        SIM.di |= (uint16_t)(1u << (ch - 1));
    } else {
        SIM.di &= (uint16_t)~(1u << (ch - 1));
    }
}

/* World model, every 20 ms. */
static void world_step(uint32_t now)
{
    const float dt = 0.02f;
    set_di(P.di_ignition, WRLD.ignition);
    set_di(P.di_key86s, WRLD.key86s || WRLD.ignition);
    if (WRLD.driving) {
        SIM.edges[P.di_speed - 1] += 1; /* 50 edges per second */
    }
    /* Roof: about 10 s from end to end. */
    bool k_open = SIM.relay[P.k_roof_a - 1], k_close = SIM.relay[P.k_roof_b - 1];
    WRLD.roof += (k_open ? dt / 10.0f : 0) - (k_close ? dt / 10.0f : 0);
    WRLD.roof = WRLD.roof < 0 ? 0 : WRLD.roof > 1 ? 1 : WRLD.roof;
    bool closed = WRLD.roof <= 0.001f, open = WRLD.roof >= 0.999f;
    set_di(P.di_roof_a, open);
    set_di(P.di_roof_b, closed);
    /* Fridge cabinet: cools fast in the simulation so the thermostat can be watched. */
    WRLD.fridge_c += SIM.relay[P.k_fridge - 1] ? -0.2f * dt : 0.05f * dt;
    /* Heater: operating indicator 5 s after S+, off 15 s after S+ ends (run-on). */
    if (SIM.relay[P.k_heater - 1]) {
        WRLD.heater_off_ms = 0;
        if (!WRLD.heater_on_ms) {
            WRLD.heater_on_ms = now;
        }
        if (now - WRLD.heater_on_ms > 5000) {
            WRLD.run = true;
        }
    } else {
        WRLD.heater_on_ms = 0;
        if (WRLD.run && !WRLD.heater_off_ms) {
            WRLD.heater_off_ms = now;
        }
        if (WRLD.heater_off_ms && now - WRLD.heater_off_ms > 15000) {
            WRLD.run = false;
            WRLD.heater_off_ms = 0;
        }
    }
    set_di(P.di_heater_run, WRLD.run);

    SIM.ai[P.ai_fridge - 1] = ntc_volts(WRLD.fridge_c);
    SIM.ai[P.ai_outside - 1] = ntc_volts(WRLD.outside_c);
    static const float fresh_ohm[4] = {1000, 650, 375, 150};
    float r = fresh_ohm[WRLD.fresh_step];
    SIM.ai[P.ai_fresh - 1] = 3.3f * r / (r + 1000.0f);
    r = WRLD.waste_full ? 200.0f : 900.0f;
    SIM.ai[P.ai_waste - 1] = 3.3f * r / (r + 1000.0f);
    SIM.ai[P.ai_ref - 1] = 2.75f;           /* 3.3 V via divider 1.2 (sim profile) */
    SIM.ai[P.ai_battery - 1] = 13.2f / 5.7f;

    energy_t *e = &CTL.energy;
    e->connected = WRLD.cerbo;
    if (WRLD.cerbo) {
        float w = WRLD.shore ? 300.0f : -120.0f;
        energy_set(e, EN_SOC, (float)WRLD.soc, now);
        energy_set(e, EN_BATTERY_V, WRLD.shore ? 13.8f : 13.1f, now);
        energy_set(e, EN_BATTERY_W, w, now);
        energy_set(e, EN_ACTIVE_INPUT, WRLD.shore ? 0 : 240, now);
        energy_set(e, EN_IN_V, 230, now);
        energy_set(e, EN_VEBUS_STATE, WRLD.shore ? 5 : 9, now);
        energy_set(e, EN_PV_W, 0, now);
        energy_set(e, EN_ALT_W, WRLD.driving ? 450.0f : 0.0f, now);
        if (WRLD.driving) {
            energy_set(e, EN_BATTERY_W, 330.0f, now);
        }
    }
    for (int i = 0; i < CU_N_DI && di_btn[i]; i++) {
        if ((SIM.di >> i) & 1u) {
            lv_obj_add_state(di_btn[i], LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(di_btn[i], LV_STATE_CHECKED);
        }
    }
}

static uint32_t now_ms(void)
{
    return lv_tick_get();
}

static void io_timer(lv_timer_t *t)
{
    (void)t;
    uint32_t now = now_ms();
    world_step(now);
    control_io_step(&CTL, now);
}

static void logic_timer(lv_timer_t *t)
{
    (void)t;
    control_logic_step(&CTL, now_ms());
}

/* ---------- control panel (simulator only, not part of the device UI) ---------- */

static void sw_cb(lv_event_t *e)
{
    bool *flag = lv_event_get_user_data(e);
    *flag = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
}

static void di_cb(lv_event_t *e)
{
    int ch = (int)(intptr_t)lv_event_get_user_data(e);
    set_di((uint8_t)ch, lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED));
}

static void slider_cb(lv_event_t *e)
{
    int which = (int)(intptr_t)lv_event_get_user_data(e);
    int32_t v = lv_slider_get_value(lv_event_get_target(e));
    if (which == 0) {
        WRLD.fridge_c = (float)v;
    } else if (which == 1) {
        WRLD.outside_c = (float)v;
    } else if (which == 2) {
        WRLD.fresh_step = (int)v;
    } else {
        WRLD.soc = (int)v;
    }
}

static lv_obj_t *small(lv_obj_t *parent, const char *s)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, s);
    lv_obj_set_style_text_color(l, lv_color_hex(0x9aa9b7), 0);
    return l;
}

static void add_switch(lv_obj_t *parent, const char *name, bool *flag)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, 150, 40);
    lv_obj_t *sw = lv_switch_create(row);
    lv_obj_align(sw, LV_ALIGN_LEFT_MID, 0, 0);
    if (*flag) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(sw, sw_cb, LV_EVENT_VALUE_CHANGED, flag);
    lv_obj_t *l = small(row, name);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 58, 0);
}

static void add_slider(lv_obj_t *parent, const char *name, int which, int32_t min, int32_t max, int32_t val)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, 300, 44);
    lv_obj_t *l = small(row, name);
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t *s = lv_slider_create(row);
    lv_obj_set_width(s, 280);
    lv_obj_align(s, LV_ALIGN_BOTTOM_LEFT, 8, -4);
    lv_slider_set_range(s, min, max);
    lv_slider_set_value(s, val, LV_ANIM_OFF);
    lv_obj_add_event_cb(s, slider_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)which);
}

static void build_panel(lv_obj_t *scr)
{
    lv_obj_t *p = lv_obj_create(scr);
    lv_obj_remove_style_all(p);
    lv_obj_set_pos(p, 0, DEV_H);
    lv_obj_set_size(p, DEV_W, PANEL_H);
    lv_obj_set_style_bg_color(p, lv_color_hex(0x05080b), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(p, 14, 0);
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(p, 6, 0);
    lv_obj_set_style_pad_column(p, 14, 0);
    lv_obj_remove_flag(p, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *head = small(p, sim_profile ? "SIMULATOR - Profil mit erfundenen Simulationswerten (profiles/sim), keine Fahrzeugdaten"
                                          : "SIMULATOR - echtes Profil: alles TODO_MEASURE, nichts freigegeben");
    lv_obj_set_width(head, DEV_W - 30);
    add_switch(p, "Zuendung", &WRLD.ignition);
    add_switch(p, "Fahrt", &WRLD.driving);
    add_switch(p, "86s", &WRLD.key86s);
    add_switch(p, "Cerbo", &WRLD.cerbo);
    add_switch(p, "Landstrom", &WRLD.shore);
    add_switch(p, "Abwasser voll", &WRLD.waste_full);
    add_slider(p, "Kuehlbox-Innentemperatur (Modell) -10..30 C", 0, -10, 30, (int32_t)WRLD.fridge_c);
    add_slider(p, "Aussentemperatur -20..40 C", 1, -20, 40, (int32_t)WRLD.outside_c);
    add_slider(p, "Frischwasser Stufe 0..3", 2, 0, 3, WRLD.fresh_step);
    add_slider(p, "SoC %", 3, 0, 100, WRLD.soc);
    lv_obj_t *dl = small(p, "DI1..DI16 (DI1/2 Dach, DI4 Heizung, DI8/10 aus dem Modell):");
    lv_obj_set_width(dl, DEV_W - 30);
    for (int i = 0; i < CU_N_DI; i++) {
        lv_obj_t *b = lv_button_create(p);
        lv_obj_set_size(b, 66, 34);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CHECKABLE);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text_fmt(l, "DI%d", i + 1);
        lv_obj_center(l);
        lv_obj_add_event_cb(b, di_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)(i + 1));
        di_btn[i] = b;
    }
}

static void touch_cb(lv_event_t *e)
{
    lv_point_t pt;
    lv_indev_get_point(lv_event_get_target(e), &pt);
    ui_touch_pressed(pt.x, pt.y);
}

/* ---------- screenshot ---------- */

static void flush_noop(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
    (void)a;
    (void)px;
    lv_display_flush_ready(d);
}

static int write_bmp(const char *path, lv_obj_t *obj)
{
    lv_draw_buf_t *buf = lv_snapshot_take(obj, LV_COLOR_FORMAT_RGB888);
    if (!buf) {
        fprintf(stderr, "snapshot failed\n");
        return 1;
    }
    uint32_t w = buf->header.w, h = buf->header.h, stride = buf->header.stride;
    uint32_t row = (w * 3 + 3) & ~3u;
    FILE *f = fopen(path, "wb");
    if (!f) {
        return 1;
    }
    uint32_t size = 54 + row * h;
    uint8_t hdr[54] = {'B', 'M'};
    memcpy(hdr + 2, &size, 4);
    hdr[10] = 54;
    hdr[14] = 40;
    memcpy(hdr + 18, &w, 4);
    memcpy(hdr + 22, &h, 4);
    hdr[26] = 1;
    hdr[28] = 24;
    fwrite(hdr, 1, 54, f);
    uint8_t pad[4] = {0};
    for (int32_t y = (int32_t)h - 1; y >= 0; y--) { /* RGB888 in LVGL is stored as B, G, R */
        fwrite(buf->data + (uint32_t)y * stride, 1, w * 3, f);
        fwrite(pad, 1, row - w * 3, f);
    }
    fclose(f);
    lv_draw_buf_destroy(buf);
    printf("wrote %s (%ux%u)\n", path, w, h);
    return 0;
}

int main(int argc, char **argv)
{
    const char *shot = NULL, *page = "overview";
    bool shot_all = false, tap_open = false;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--shot") && i + 1 < argc) {
            shot = argv[++i];
        } else if (!strcmp(argv[i], "--tap-open")) {
            tap_open = true;
        } else if (!strcmp(argv[i], "--shot-all")) {
            shot_all = true;
        } else if (!strcmp(argv[i], "--page") && i + 1 < argc) {
            page = argv[++i];
        } else if (!strcmp(argv[i], "--profile") && i + 1 < argc) {
            sim_profile = strcmp(argv[++i], "real") != 0;
        }
    }

    char *base = slurp(CU_PROFILE_DIR "/t6-california-a8l-2018.json");
    char *ov = sim_profile ? slurp(CU_PROFILE_DIR "/sim/sim-overrides.json") : NULL;
    char err[160];
    if (!base || !profile_parse_with_overlay(base, ov, &P, err, sizeof err)) {
        fprintf(stderr, "profile: %s\n", base ? err : "not found");
        return 1;
    }
    free(base);
    free(ov);

    lv_init();
    lv_display_t *disp;
    if (shot) {
        lv_tick_set_cb(tick_cb);
        disp = lv_display_create(DEV_W, DEV_H + PANEL_H);
        static uint8_t fb[DEV_W * 64 * 4];
        lv_display_set_buffers(disp, fb, NULL, sizeof fb, LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_display_set_flush_cb(disp, flush_noop);
    } else {
        disp = lv_sdl_window_create(DEV_W, DEV_H + PANEL_H);
        lv_indev_t *mouse = lv_sdl_mouse_create();
        lv_indev_add_event_cb(mouse, touch_cb, LV_EVENT_PRESSED, NULL);
        lv_sdl_keyboard_create();
    }
    (void)disp;
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x05080b), 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    hal_sim_init(&SIM, &IO);
    state_bus_init(&BUS);
    control_init(&CTL, &P, &IO, &BUS, (cu_ms_t)lv_tick_get());
    cu_cmd_t on = {.type = CMD_FRIDGE, .on = true, .value = 5};
    state_bus_push(&BUS, &on);

    lv_obj_t *dev = lv_obj_create(scr);
    lv_obj_remove_style_all(dev);
    lv_obj_set_size(dev, DEV_W, DEV_H);
    ui_config_t cfg = {.bus = &BUS, .simulation = true, .return_min = P.ui_return_min};
    ui_init(dev, &cfg);
    build_panel(scr);
    lv_timer_create(io_timer, 20, NULL);
    lv_timer_create(logic_timer, 50, NULL);

    if (!strcmp(page, "roof")) {
        ui_show(UI_PAGE_ROOF);
    } else if (!strcmp(page, "fridge")) {
        ui_show(UI_PAGE_FRIDGE);
    }

    if (shot) {
        for (int i = 0; i < 400; i++) { /* 4 s simulated */
            fake_ms += 10;
            lv_timer_handler();
            if (tap_open && i == 100) {
                ui_roof_tap(ROOF_OPEN);
            }
        }
        return write_bmp(shot, shot_all ? scr : ui_root());
    }
    while (1) {
        uint32_t wait = lv_timer_handler();
        if (wait > 10) {
            wait = 10;
        }
        lv_delay_ms(wait);
    }
    return 0;
}
