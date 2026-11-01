/* LVGL interface after the current web UI ui/reference/web_v1/ (dark theme).
 * M1: header, overview, roof and fridge pages; the other pages follow in M2. */
#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "i18n.h"
#include "modules.h"

LV_FONT_DECLARE(font_15_semi);
LV_FONT_DECLARE(font_16_semi);
LV_FONT_DECLARE(font_18);
LV_FONT_DECLARE(font_20_semi);
LV_FONT_DECLARE(font_22_semi);
LV_FONT_DECLARE(font_28_semi);
LV_FONT_DECLARE(font_34_semi);
LV_FONT_DECLARE(font_44_bold);
LV_FONT_DECLARE(font_46_semi);
LV_FONT_DECLARE(font_56_semi);
LV_FONT_DECLARE(font_84_semi);

/* Colours of the reference (dark theme). */
#define C_BG 0x0f1419
#define C_SURFACE 0x182029
#define C_SURFACE2 0x212b36
#define C_LINE 0x2c3845
#define C_INK 0xeef3f7
#define C_MUTED 0x9aa9b7
#define C_ACCENT 0x4fb3d9
#define C_ACCENT_INK 0x06202b
#define C_OK 0x4cc38a
#define C_WARN 0xf0b429
#define C_ERR 0xef6262

#define W 1280
#define H 480
#define HEAD 64
#define PAD 16
#define GAP 12
#define BODY_W (W - 2 * PAD)
#define BODY_H (H - HEAD - 2 * PAD)

typedef enum { K_NEUTRAL = 0, K_OK, K_WARN, K_ERR, K_ACCENT } kind_t;

typedef struct {
    lv_obj_t *box, *dot, *text;
} chip_t;

typedef struct {
    lv_obj_t *obj, *big, *sub;
    chip_t chip;
    lv_obj_t *steps[3];
} tile_t;

static struct {
    ui_config_t cfg;
    lv_obj_t *root, *body;
    lv_obj_t *back, *brand, *title, *outside, *dot_unit, *dot_cerbo, *clock;
    lv_obj_t *pages[UI_PAGE_COUNT];
    ui_page_t page;
    tile_t tiles[5];    /* fridge, heat, water, roof (index 0 unused: energy tile below) */
    struct {
        lv_obj_t *batt, *fill, *pct, *bolt, *status, *sub, *chips;
        chip_t chip[2];
    } en;
    cu_state_t st;
    /* roof page */
    lv_obj_t *rf_pos, *rf_note, *rf_btn[2], *rf_btn_label[2], *rf_btn_hint[2], *rf_stop;
    chip_t rf_chip;
    roof_dir_t roof_dir;  /* direction the UI is confirming (ROOF_STOP = none) */
    bool roof_auto;       /* tap run */
    uint32_t roof_seq;
    uint32_t roof_started;
    /* fridge page */
    lv_obj_t *fr_temp, *fr_target, *fr_minus, *fr_plus, *fr_toggle, *fr_toggle_label, *fr_rows[5];
    chip_t fr_chip;
} U;

/* ---------- small helpers ---------- */

static lv_obj_t *box(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t bg, int32_t radius)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static void border(lv_obj_t *o, uint32_t color, int32_t width)
{
    lv_obj_set_style_border_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_border_width(o, width, 0);
}

static lv_obj_t *text(lv_obj_t *parent, const lv_font_t *font, uint32_t color, const char *s)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, s);
    return l;
}

static void set_text(lv_obj_t *l, const char *s)
{
    if (strcmp(lv_label_get_text(l), s) != 0) {
        lv_label_set_text(l, s);
    }
}

/* "{name}" -> value, all occurrences of one placeholder. */
static const char *fill(char *buf, size_t len, const char *tmpl, const char *name, const char *value)
{
    char key[16];
    snprintf(key, sizeof key, "{%s}", name);
    const char *p = strstr(tmpl, key);
    if (!p) {
        snprintf(buf, len, "%s", tmpl);
        return buf;
    }
    snprintf(buf, len, "%.*s%s%s", (int)(p - tmpl), tmpl, value, p + strlen(key));
    return buf;
}

/* Number with a decimal comma (de-CH as in the reference), "–" for NAN. */
static const char *num(char *buf, size_t len, float v, int decimals, const char *unit)
{
    if (isnan(v)) {
        snprintf(buf, len, "\xe2\x80\x93"); /* – */
        return buf;
    }
    snprintf(buf, len, "%.*f%s%s", decimals, (double)v, unit[0] ? " " : "", unit);
    char *dot = strchr(buf, '.');
    if (dot) {
        *dot = ',';
    }
    char *minus = strchr(buf, '-');
    if (minus == buf) { /* typographic minus */
        memmove(buf + 3, buf + 1, strlen(buf));
        memcpy(buf, "\xe2\x88\x92", 3);
    }
    return buf;
}

static uint32_t kind_color(kind_t k)
{
    switch (k) {
    case K_OK: return C_OK;
    case K_WARN: return C_WARN;
    case K_ERR: return C_ERR;
    case K_ACCENT: return C_ACCENT;
    default: return C_MUTED;
    }
}

static chip_t chip(lv_obj_t *parent)
{
    chip_t c;
    c.box = box(parent, 0, 0, LV_SIZE_CONTENT, 34, C_SURFACE2, 17);
    lv_obj_set_style_pad_hor(c.box, 14, 0);
    lv_obj_set_flex_flow(c.box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(c.box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(c.box, 8, 0);
    c.dot = box(c.box, 0, 0, 9, 9, C_MUTED, LV_RADIUS_CIRCLE);
    c.text = text(c.box, &font_16_semi, C_INK, "\xe2\x80\x93");
    return c;
}

static void set_chip(chip_t *c, const char *s, kind_t k)
{
    set_text(c->text, s);
    lv_obj_set_style_bg_color(c->dot, lv_color_hex(kind_color(k)), 0);
    lv_obj_set_style_text_color(c->text, lv_color_hex(k == K_ERR ? C_ERR : C_INK), 0);
}

static lv_obj_t *panel(lv_obj_t *parent, int32_t x, int32_t w)
{
    lv_obj_t *p = box(parent, x, 0, w, BODY_H, C_SURFACE, 16);
    border(p, C_LINE, 1);
    lv_obj_set_style_pad_all(p, 20, 0);
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(p, 10, 0);
    return p;
}

static lv_obj_t *label_caps(lv_obj_t *parent, tx_t id)
{
    char up[96];
    snprintf(up, sizeof up, "%s", tr(id));
    /* Uppercase ASCII and the umlauts used in the labels. */
    for (char *s = up; *s; s++) {
        if (*s >= 'a' && *s <= 'z') {
            *s = (char)(*s - 32);
        } else if ((unsigned char)s[0] == 0xc3 && s[1]) {
            unsigned char n = (unsigned char)s[1];
            if (n == 0xa4 || n == 0xb6 || n == 0xbc) { /* ä ö ü -> Ä Ö Ü */
                s[1] = (char)(n - 0x20);
            }
            s++;
        }
    }
    lv_obj_t *l = text(parent, &font_15_semi, C_MUTED, up);
    lv_obj_set_style_text_letter_space(l, 1, 0);
    return l;
}

/* ---------- commands ---------- */

static void send(const cu_cmd_t *c)
{
    state_bus_push(U.cfg.bus, c);
}

static void roof_send(roof_dir_t dir, bool auto_run)
{
    cu_cmd_t c = {.type = CMD_ROOF, .seq = ++U.roof_seq, .dir = dir, .auto_run = auto_run};
    send(&c);
}

static void roof_stop(void)
{
    if (U.roof_dir != ROOF_STOP) {
        U.roof_dir = ROOF_STOP;
        U.roof_auto = false;
        roof_send(ROOF_STOP, false);
    }
}

/* Run messages every 100 ms while the UI wants the roof to move (S3). */
static void roof_timer(lv_timer_t *t)
{
    (void)t;
    if (U.roof_dir == ROOF_STOP) {
        return;
    }
    /* Tap run: stop confirming once the logic has stopped (end position or a lock). */
    if (U.roof_auto && lv_tick_elaps(U.roof_started) > 800 && U.st.roof.active == ROOF_STOP) {
        U.roof_dir = ROOF_STOP;
        U.roof_auto = false;
        return;
    }
    roof_send(U.roof_dir, U.roof_auto);
}

static void roof_btn_cb(lv_event_t *e)
{
    roof_dir_t dir = (roof_dir_t)(intptr_t)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    bool tap = U.st.roof.tap;
    if (tap) {
        if (code == LV_EVENT_CLICKED && U.roof_dir == ROOF_STOP) {
            U.roof_dir = dir;
            U.roof_auto = true;
            U.roof_started = lv_tick_get();
            roof_send(dir, true);
        }
        return;
    }
    if (code == LV_EVENT_PRESSED) { /* hold to run */
        U.roof_dir = dir;
        U.roof_auto = false;
        U.roof_started = lv_tick_get();
        roof_send(dir, false);
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        roof_stop();
    }
}

void ui_roof_tap(roof_dir_t dir)
{
    lv_obj_send_event(U.rf_btn[dir == ROOF_OPEN ? 0 : 1], LV_EVENT_CLICKED, NULL);
}

static void stop_cb(lv_event_t *e)
{
    (void)e;
    roof_stop();
}

void ui_touch_pressed(int32_t x, int32_t y)
{
    lv_area_t a;
    lv_obj_get_coords(U.root, &a);
    if (x < a.x1 || x > a.x2 || y < a.y1 || y > a.y2) {
        return;
    }
    /* Any touch stops a tap-started run (the first touch after the start). */
    if (U.roof_auto && U.roof_dir != ROOF_STOP && lv_tick_elaps(U.roof_started) > 150) {
        roof_stop();
    }
}

static void fridge_cmd(bool on, float target)
{
    cu_cmd_t c = {.type = CMD_FRIDGE, .on = on, .value = target};
    send(&c);
}

static void fridge_btn_cb(lv_event_t *e)
{
    int what = (int)(intptr_t)lv_event_get_user_data(e);
    const cu_state_t *s = &U.st;
    if (what == 0) {
        fridge_cmd(!s->fridge.on, s->fridge.target_c);
    } else if (!isnan(s->fridge.target_c)) {
        fridge_cmd(s->fridge.on, s->fridge.target_c + (float)what);
    }
}

/* ---------- navigation ---------- */

static const tx_t page_title[UI_PAGE_COUNT] = {TX_PAGE_OVERVIEW, TX_PAGE_ROOF,  TX_PAGE_FRIDGE, TX_PAGE_ENERGY,
                                               TX_PAGE_HEAT,     TX_PAGE_POWER, TX_PAGE_WATER,  TX_PAGE_SETTINGS};

void ui_show(ui_page_t page)
{
    if (page >= UI_PAGE_COUNT) {
        return;
    }
    if (U.page == UI_PAGE_ROOF && page != UI_PAGE_ROOF) {
        roof_stop(); /* leaving the roof page stops the roof */
    }
    U.page = page;
    for (int i = 0; i < UI_PAGE_COUNT; i++) {
        if (i == (int)page) {
            lv_obj_remove_flag(U.pages[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(U.pages[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    bool ov = page == UI_PAGE_OVERVIEW;
    if (ov) {
        lv_obj_add_flag(U.back, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(U.brand, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(U.back, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(U.brand, LV_OBJ_FLAG_HIDDEN);
    }
    set_text(U.title, tr(page_title[page]));
}

static void go_cb(lv_event_t *e)
{
    ui_show((ui_page_t)(intptr_t)lv_event_get_user_data(e));
}

lv_obj_t *ui_root(void)
{
    return U.root;
}

/* ---------- header ---------- */

static lv_obj_t *dot_link(lv_obj_t *parent, tx_t name, lv_obj_t **dot)
{
    lv_obj_t *row = box(parent, 0, 0, LV_SIZE_CONTENT, LV_SIZE_CONTENT, C_BG, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, 0);
    *dot = box(row, 0, 0, 10, 10, C_OK, LV_RADIUS_CIRCLE);
    text(row, &font_15_semi, C_MUTED, tr(name));
    return row;
}

static void build_header(void)
{
    lv_obj_t *h = box(U.root, 0, 0, W, HEAD, C_BG, 0);
    lv_obj_set_style_border_side(h, LV_BORDER_SIDE_BOTTOM, 0);
    border(h, C_LINE, 1);
    lv_obj_set_style_pad_hor(h, PAD, 0);
    lv_obj_set_flex_flow(h, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(h, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(h, 16, 0);

    U.back = box(h, 0, 0, LV_SIZE_CONTENT, 52, C_SURFACE2, 12);
    lv_obj_set_style_pad_left(U.back, 16, 0);
    lv_obj_set_style_pad_right(U.back, 22, 0);
    lv_obj_set_flex_flow(U.back, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(U.back, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(U.back, 8, 0);
    lv_obj_t *chev = text(U.back, &lv_font_montserrat_24, C_INK, LV_SYMBOL_LEFT);
    (void)chev;
    text(U.back, &font_20_semi, C_INK, tr(TX_PAGE_OVERVIEW));
    lv_obj_add_flag(U.back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(U.back, go_cb, LV_EVENT_CLICKED, (void *)(intptr_t)UI_PAGE_OVERVIEW);

    U.brand = text(h, &font_20_semi, C_INK, tr(TX_BRAND));
    lv_obj_set_style_text_letter_space(U.brand, 4, 0);
    lv_obj_set_style_pad_left(U.brand, 6, 0);
    U.title = text(h, &font_22_semi, C_INK, tr(TX_PAGE_OVERVIEW));

    lv_obj_t *spacer = box(h, 0, 0, 1, 1, C_BG, 0);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);

    if (U.cfg.simulation) {
        chip_t sim = chip(h);
        set_chip(&sim, tr(TX_SIM_BADGE), K_WARN);
    }
    lv_obj_t *out = box(h, 0, 0, LV_SIZE_CONTENT, LV_SIZE_CONTENT, C_BG, 0);
    lv_obj_set_flex_flow(out, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(out, 8, 0);
    text(out, &font_18, C_MUTED, tr(TX_TOP_OUTSIDE));
    U.outside = text(out, &font_18, C_INK, "\xe2\x80\x93");
    dot_link(h, TX_TOP_UNIT, &U.dot_unit);
    dot_link(h, TX_TOP_VICTRON, &U.dot_cerbo);
    U.clock = text(h, &font_18, C_MUTED, "--:--");
    lv_obj_t *gear = box(h, 0, 0, 52, 52, C_SURFACE2, 12);
    lv_obj_t *g = text(gear, &lv_font_montserrat_24, C_INK, LV_SYMBOL_SETTINGS);
    lv_obj_center(g);
    lv_obj_add_flag(gear, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(gear, go_cb, LV_EVENT_CLICKED, (void *)(intptr_t)UI_PAGE_SETTINGS);
}

/* ---------- overview ---------- */

#define TILE_W ((BODY_W - 2 * GAP) / 3)
#define TILE_H ((BODY_H - GAP) / 2)

static lv_obj_t *tile_box(lv_obj_t *parent, int col, int row, int span, tx_t label, ui_page_t target)
{
    lv_obj_t *t = box(parent, col * (TILE_W + GAP), row * (TILE_H + GAP), span * TILE_W + (span - 1) * GAP, TILE_H,
                      C_SURFACE, 18);
    border(t, C_LINE, 1);
    lv_obj_set_style_pad_hor(t, 22, 0);
    lv_obj_set_style_pad_ver(t, 18, 0);
    lv_obj_set_flex_flow(t, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(t, 6, 0);
    lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(t, lv_color_hex(C_SURFACE2), LV_STATE_PRESSED);
    lv_obj_add_event_cb(t, go_cb, LV_EVENT_CLICKED, (void *)(intptr_t)target);
    label_caps(t, label);
    lv_obj_t *chev = text(t, &lv_font_montserrat_24, C_MUTED, LV_SYMBOL_RIGHT);
    lv_obj_add_flag(chev, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_align(chev, LV_ALIGN_TOP_RIGHT, 0, -2);
    return t;
}

static void filler(lv_obj_t *parent)
{
    lv_obj_t *grow = box(parent, 0, 0, 1, 1, C_SURFACE, 0);
    lv_obj_set_style_bg_opa(grow, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_grow(grow, 1);
}

static tile_t make_tile(lv_obj_t *parent, int col, int row, tx_t label, ui_page_t target, bool steps)
{
    tile_t t = {0};
    t.obj = tile_box(parent, col, row, 1, label, target);
    t.big = text(t.obj, &font_46_semi, C_INK, "\xe2\x80\x93");
    if (steps) { /* water: three level bars instead of a sub line */
        lv_obj_t *r = box(t.obj, 0, 0, LV_PCT(100), 14, C_SURFACE, 0);
        lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(r, 6, 0);
        for (int k = 0; k < 3; k++) {
            t.steps[k] = box(r, 0, 0, 0, 14, C_SURFACE2, 5);
            lv_obj_set_flex_grow(t.steps[k], 1);
        }
    } else {
        t.sub = text(t.obj, &font_18, C_MUTED, "\xe2\x80\x93");
    }
    filler(t.obj);
    t.chip = chip(t.obj);
    return t;
}

/* Battery graphic as in the web UI: fill = SoC, colour by level, bolt while charging. */
static void build_battery(lv_obj_t *parent)
{
    lv_obj_t *wrap = box(parent, 0, 0, 264, 104, C_SURFACE, 0);
    lv_obj_set_style_bg_opa(wrap, LV_OPA_TRANSP, 0);
    U.en.batt = box(wrap, 0, 0, 248, 104, C_SURFACE, 18);
    border(U.en.batt, C_INK, 5);
    lv_obj_set_style_pad_all(U.en.batt, 6, 0);
    lv_obj_t *nub = box(wrap, 253, 32, 10, 40, C_INK, 6);
    (void)nub;
    U.en.fill = box(U.en.batt, 0, 0, 0, LV_PCT(100), C_OK, 10);
    lv_obj_t *lab = box(U.en.batt, 0, 0, LV_PCT(100), LV_PCT(100), C_SURFACE, 0);
    lv_obj_set_style_bg_opa(lab, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(lab, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(lab, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(lab, 4, 0);
    U.en.bolt = text(lab, &lv_font_montserrat_24, C_INK, LV_SYMBOL_CHARGE);
    U.en.pct = text(lab, &font_44_bold, C_INK, "\xe2\x80\x93");
}

static void build_overview(lv_obj_t *page)
{
    /* Row 1: "Batterie und Strom" over two columns, fridge. Row 2: heater, water, roof. */
    lv_obj_t *en = tile_box(page, 0, 0, 2, TX_PAGE_ENERGY, UI_PAGE_ENERGY);
    lv_obj_t *body = box(en, 0, 0, LV_PCT(100), 0, C_SURFACE, 0);
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(body, 36, 0);
    build_battery(body);
    lv_obj_t *info = box(body, 0, 0, 0, LV_SIZE_CONTENT, C_SURFACE, 0);
    lv_obj_set_style_bg_opa(info, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_grow(info, 1);
    lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(info, 8, 0);
    U.en.status = text(info, &font_34_semi, C_INK, "\xe2\x80\x93");
    U.en.sub = text(info, &font_18, C_MUTED, "");
    U.en.chips = box(info, 0, 0, LV_PCT(100), 34, C_SURFACE, 0);
    lv_obj_set_style_bg_opa(U.en.chips, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(U.en.chips, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(U.en.chips, 8, 0);
    U.en.chip[0] = chip(U.en.chips);
    U.en.chip[1] = chip(U.en.chips);

    U.tiles[1] = make_tile(page, 2, 0, TX_TILE_FRIDGE, UI_PAGE_FRIDGE, false);
    U.tiles[2] = make_tile(page, 0, 1, TX_TILE_HEAT, UI_PAGE_HEAT, false);
    U.tiles[3] = make_tile(page, 1, 1, TX_TILE_WATER, UI_PAGE_WATER, true);
    U.tiles[4] = make_tile(page, 2, 1, TX_TILE_ROOF, UI_PAGE_ROOF, false);
}

/* ---------- roof page ---------- */

static void build_roof(lv_obj_t *page)
{
    int32_t lw = (BODY_W - GAP) * 2 / 5, rw = BODY_W - GAP - lw;
    lv_obj_t *p = panel(page, 0, lw);
    lv_obj_set_style_pad_row(p, 12, 0);
    label_caps(p, TX_ROOF_POSITION);
    U.rf_pos = text(p, &font_56_semi, C_INK, "\xe2\x80\x93");
    U.rf_chip = chip(p);
    lv_obj_t *grow = box(p, 0, 0, 1, 1, C_SURFACE, 0);
    lv_obj_set_flex_grow(grow, 1);
    U.rf_note = text(p, &font_15_semi, C_MUTED, "");
    lv_obj_set_width(U.rf_note, LV_PCT(100));
    lv_label_set_long_mode(U.rf_note, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(U.rf_note, 4, 0);

    int32_t bw = (rw - GAP) / 2;
    static const char *const arrow[2] = {LV_SYMBOL_UP, LV_SYMBOL_DOWN};
    static const tx_t name[2] = {TX_ROOF_OPEN, TX_ROOF_CLOSE};
    for (int i = 0; i < 2; i++) {
        lv_obj_t *b = box(page, lw + GAP + i * (bw + GAP), 0, bw, BODY_H, C_SURFACE, 18);
        border(b, C_LINE, 2);
        lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(b, 14, 0);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        text(b, &lv_font_montserrat_48, C_INK, arrow[i]);
        U.rf_btn_label[i] = text(b, &font_28_semi, C_INK, tr(name[i]));
        U.rf_btn_hint[i] = text(b, &font_18, C_MUTED, tr(TX_ROOF_TAP));
        roof_dir_t dir = i == 0 ? ROOF_OPEN : ROOF_CLOSE;
        lv_obj_add_event_cb(b, roof_btn_cb, LV_EVENT_PRESSED, (void *)(intptr_t)dir);
        lv_obj_add_event_cb(b, roof_btn_cb, LV_EVENT_RELEASED, (void *)(intptr_t)dir);
        lv_obj_add_event_cb(b, roof_btn_cb, LV_EVENT_PRESS_LOST, (void *)(intptr_t)dir);
        lv_obj_add_event_cb(b, roof_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)dir);
        U.rf_btn[i] = b;
    }
    /* Tap run: one red STOP over both keys; any touch anywhere stops as well. */
    U.rf_stop = box(page, lw + GAP, 0, rw, BODY_H, C_ERR, 18);
    lv_obj_set_flex_flow(U.rf_stop, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(U.rf_stop, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(U.rf_stop, 14, 0);
    lv_obj_t *st = text(U.rf_stop, &font_46_semi, 0xffffff, tr(TX_ROOF_STOP));
    lv_obj_set_style_text_letter_space(st, 2, 0);
    text(U.rf_stop, &font_18, 0xffffff, tr(TX_ROOF_STOP_HINT));
    lv_obj_add_flag(U.rf_stop, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(U.rf_stop, stop_cb, LV_EVENT_PRESSED, NULL);
}

/* ---------- fridge page ---------- */

static lv_obj_t *row(lv_obj_t *parent, tx_t name)
{
    lv_obj_t *r = box(parent, 0, 0, LV_PCT(100), LV_SIZE_CONTENT, C_SURFACE, 0);
    lv_obj_set_style_border_side(r, LV_BORDER_SIDE_TOP, 0);
    border(r, C_LINE, 1);
    lv_obj_set_style_pad_ver(r, 7, 0);
    lv_obj_t *n = text(r, &font_16_semi, C_MUTED, tr(name));
    lv_obj_align(n, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *v = text(r, &font_16_semi, C_INK, "\xe2\x80\x93");
    lv_obj_align(v, LV_ALIGN_RIGHT_MID, 0, 0);
    return v;
}

static lv_obj_t *step_btn(lv_obj_t *parent, const char *sym, int delta)
{
    lv_obj_t *b = box(parent, 0, 0, 112, 112, C_SURFACE2, 20);
    lv_obj_t *l = text(b, &font_46_semi, C_INK, sym);
    lv_obj_center(l);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(b, lv_color_hex(C_LINE), LV_STATE_PRESSED);
    lv_obj_set_style_opa(b, LV_OPA_40, LV_STATE_DISABLED);
    lv_obj_add_event_cb(b, fridge_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)delta);
    return b;
}

static void build_fridge(lv_obj_t *page)
{
    int32_t lw = (BODY_W - GAP) * 4 / 9, rw = BODY_W - GAP - lw;
    lv_obj_t *p = panel(page, 0, lw);
    lv_obj_set_style_pad_row(p, 6, 0);
    label_caps(p, TX_FRIDGE_INSIDE);
    U.fr_temp = text(p, &font_84_semi, C_INK, "\xe2\x80\x93");
    U.fr_chip = chip(p);
    lv_obj_t *grow = box(p, 0, 0, 1, 1, C_SURFACE, 0);
    lv_obj_set_flex_grow(grow, 1);
    static const tx_t rows[5] = {TX_FRIDGE_ROW_RAW, TX_FRIDGE_ROW_REQUEST, TX_FRIDGE_ROW_OUTPUT,
                                 TX_FRIDGE_ROW_COMPRESSOR, TX_FRIDGE_ROW_DIAG};
    for (int i = 0; i < 5; i++) {
        U.fr_rows[i] = row(p, rows[i]);
    }

    lv_obj_t *q = panel(page, lw + GAP, rw);
    label_caps(q, TX_FRIDGE_TARGET);
    lv_obj_t *stepper = box(q, 0, 0, LV_PCT(100), 112, C_SURFACE, 0);
    U.fr_minus = step_btn(stepper, "\xe2\x88\x92", -1);
    lv_obj_align(U.fr_minus, LV_ALIGN_LEFT_MID, 0, 0);
    U.fr_target = text(stepper, &font_84_semi, C_INK, "\xe2\x80\x93");
    lv_obj_align(U.fr_target, LV_ALIGN_CENTER, 0, 0);
    U.fr_plus = step_btn(stepper, "+", 1);
    lv_obj_align(U.fr_plus, LV_ALIGN_RIGHT_MID, 0, 0);
    U.fr_toggle = box(q, 0, 0, LV_PCT(100), 88, C_SURFACE2, 16);
    lv_obj_add_flag(U.fr_toggle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(U.fr_toggle, fridge_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)0);
    U.fr_toggle_label = text(U.fr_toggle, &font_22_semi, C_INK, "");
    lv_obj_center(U.fr_toggle_label);
    lv_obj_t *grow2 = box(q, 0, 0, 1, 1, C_SURFACE, 0);
    lv_obj_set_flex_grow(grow2, 1);
    lv_obj_t *note = text(q, &font_15_semi, C_MUTED, tr(TX_FRIDGE_NOTE));
    lv_obj_set_width(note, LV_PCT(100));
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
}

static void build_placeholder(lv_obj_t *page)
{
    lv_obj_t *p = panel(page, 0, BODY_W);
    text(p, &font_18, C_MUTED, tr(TX_COMMON_TODO_M2));
}

/* ---------- update from the state snapshot ---------- */

static const char *roof_pos_text(const cu_state_t *s)
{
    if (s->roof.active == ROOF_OPEN) {
        return tr(TX_ROOF_POS_OPENING);
    }
    if (s->roof.active == ROOF_CLOSE) {
        return tr(TX_ROOF_POS_CLOSING);
    }
    switch (s->roof.pos) {
    case ROOF_POS_CLOSED: return tr(TX_ROOF_POS_CLOSED);
    case ROOF_POS_OPEN: return tr(TX_ROOF_POS_OPEN);
    case ROOF_POS_BETWEEN: return tr(TX_ROOF_POS_BETWEEN);
    default: return tr(TX_ROOF_POS_UNKNOWN);
    }
}

static const tx_t roof_block_text[] = {
    [ROOF_BLOCK_NOT_RELEASED] = TX_ROOF_BLOCK_NOT_RELEASED, [ROOF_BLOCK_LOCKED] = TX_ROOF_BLOCK_LOCKED,
    [ROOF_BLOCK_DIRECTION] = TX_ROOF_BLOCK_DIRECTION,       [ROOF_BLOCK_DRIVE] = TX_ROOF_BLOCK_DRIVE,
    [ROOF_BLOCK_IGNITION] = TX_ROOF_BLOCK_IGNITION,         [ROOF_BLOCK_MOVING] = TX_ROOF_BLOCK_MOVING,
    [ROOF_BLOCK_INPUTS] = TX_ROOF_BLOCK_INPUTS,             [ROOF_BLOCK_OUTPUTS] = TX_ROOF_BLOCK_OUTPUTS,
    [ROOF_BLOCK_NO_CONFIRM] = TX_ROOF_BLOCK_NO_CONFIRM,     [ROOF_BLOCK_MAX_RUN] = TX_ROOF_BLOCK_MAX_RUN,
    [ROOF_BLOCK_AT_END] = TX_ROOF_BLOCK_AT_END,
};

static void roof_chip(const cu_state_t *s, const char **txt, kind_t *k)
{
    if (s->alerts & ALERT_ROOF_OPEN_DRIVING) {
        *txt = tr(TX_ROOF_CHIP_MOVING_OPEN);
        *k = K_ERR;
    } else if (!s->roof.released) {
        *txt = tr(TX_ROOF_BLOCK_NOT_RELEASED);
        *k = K_WARN;
    } else if (s->roof.active != ROOF_STOP) {
        *txt = tr(TX_ROOF_CHIP_RUNNING);
        *k = K_ACCENT;
    } else if (s->roof.busy) {
        *txt = tr(TX_ROOF_CHIP_AFTERRUN);
        *k = K_ACCENT;
    } else if (s->roof.block != ROOF_OK && s->roof.block < CU_ARRAY_LEN(roof_block_text)) {
        *txt = tr(roof_block_text[s->roof.block]);
        *k = s->roof.block == ROOF_BLOCK_NO_CONFIRM || s->roof.block == ROOF_BLOCK_AT_END ? K_NEUTRAL : K_WARN;
    } else if (!s->vehicle.ignition) {
        *txt = tr(TX_ROOF_BLOCK_IGNITION);
        *k = K_WARN;
    } else if (s->vehicle.moving) {
        *txt = tr(TX_ROOF_BLOCK_MOVING);
        *k = K_WARN;
    } else if (s->roof.pos == ROOF_POS_OPEN || s->roof.pos == ROOF_POS_CLOSED) {
        *txt = tr(TX_ROOF_CHIP_ENDPOS);
        *k = K_OK;
    } else {
        *txt = tr(TX_ROOF_CHIP_STOPPED);
        *k = K_NEUTRAL;
    }
}

static void fridge_status(const cu_state_t *s, const char **txt, kind_t *k)
{
    switch (s->fridge.thermo) {
    case FRIDGE_OFF: *txt = tr(TX_FRIDGE_STATE_OFF); *k = K_NEUTRAL; break;
    case FRIDGE_UNCALIBRATED: *txt = tr(TX_FRIDGE_STATE_UNCAL); *k = K_WARN; break;
    case FRIDGE_SENSOR_FAULT: *txt = tr(TX_FRIDGE_STATE_SENSOR); *k = K_ERR; break;
    case FRIDGE_IDLE: *txt = tr(TX_FRIDGE_STATE_IDLE); *k = K_OK; break;
    case FRIDGE_DEMAND: *txt = tr(TX_FRIDGE_REQUESTED); *k = K_ACCENT; break;
    case FRIDGE_PROTECT_PAUSE: *txt = tr(TX_FRIDGE_STATE_PROTECT_PAUSE); *k = K_WARN; break;
    case FRIDGE_PROTECT_RUN: *txt = tr(TX_FRIDGE_STATE_PROTECT_RUN); *k = K_ACCENT; break;
    }
}

/* Big tile text: falls back to a smaller font if it does not fit the tile. */
static void set_big(lv_obj_t *l, const char *s)
{
    set_text(l, s);
    const lv_font_t *f = &font_46_semi;
    int32_t max = (BODY_W - 2 * GAP) / 3 - 2 * 22 - 2;
    if (lv_text_get_width(s, strlen(s), f, 0) > max) {
        f = &font_28_semi;
    }
    if (lv_obj_get_style_text_font(l, 0) != f) {
        lv_obj_set_style_text_font(l, f, 0);
    }
}

/* Battery status text as in the web UI (v1 batteryStatus). */
static tx_t battery_status(const cu_state_t *s, bool *charging)
{
    float bw = s->energy.v[EN_BATTERY_W], pv = s->energy.v[EN_PV_W], alt = s->energy.v[EN_ALT_W];
    float state = s->energy.v[EN_VEBUS_STATE], active = s->energy.v[EN_ACTIVE_INPUT];
    bool shore = s->energy.q[EN_ACTIVE_INPUT] == CU_Q_OK && active != 240;
    bool st_ok = s->energy.q[EN_VEBUS_STATE] == CU_Q_OK;
    *charging = bw > 5;
    if (shore && st_ok && (state == 3 || state == 4 || state == 6 || state == 7)) {
        return TX_BAT_STATUS_CHARGING_SHORE;
    }
    if (shore && st_ok && state == 5) {
        return TX_BAT_STATUS_FULL_SHORE;
    }
    if (shore && st_ok && (state == 8 || state == 11) && fabsf(bw) < 10) {
        return TX_BAT_STATUS_PASSTHRU;
    }
    if (bw > 5 && s->energy.q[EN_ALT_W] == CU_Q_OK && alt > 5) {
        return TX_BAT_STATUS_CHARGING_ALT;
    }
    if (bw > 5 && s->energy.q[EN_PV_W] == CU_Q_OK && pv > 0) {
        return TX_BAT_STATUS_CHARGING_SOLAR;
    }
    if (bw > 5) {
        return TX_BAT_STATUS_CHARGING;
    }
    return bw < -5 ? TX_BAT_STATUS_DISCHARGING : TX_BAT_STATUS_IDLE;
}

static void update_energy_tile(const cu_state_t *s)
{
    char a[64], b[64], c[160];
    bool soc_ok = s->energy.q[EN_SOC] == CU_Q_OK;
    float soc = soc_ok ? s->energy.v[EN_SOC] : NAN;
    set_text(U.en.pct, soc_ok ? (snprintf(a, sizeof a, "%.0f %%", (double)soc), a) : "\xe2\x80\x93");
    int32_t inner = 248 - 2 * 5 - 2 * 6;
    lv_obj_set_width(U.en.fill, soc_ok ? (int32_t)(inner * (soc < 0 ? 0 : soc > 100 ? 100 : soc) / 100) : 0);
    lv_obj_set_style_bg_color(U.en.fill, lv_color_hex(soc < 15 ? C_ERR : soc < 30 ? C_WARN : C_OK), 0);
    bool bw_ok = s->energy.connected && s->energy.q[EN_BATTERY_W] == CU_Q_OK;
    bool charging = false;
    if (!s->energy.connected) {
        set_text(U.en.status, tr(TX_COMMON_NO_VICTRON));
    } else if (!bw_ok) {
        set_text(U.en.status, tr(TX_COMMON_NO_DATA));
    } else {
        set_text(U.en.status, tr(battery_status(s, &charging)));
    }
    if (charging) {
        lv_obj_remove_flag(U.en.bolt, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(U.en.bolt, LV_OBJ_FLAG_HIDDEN);
    }
    if (bw_ok) {
        num(a, sizeof a, s->energy.v[EN_BATTERY_W], 0, "W");
        if (s->energy.v[EN_BATTERY_W] > 0) {
            snprintf(b, sizeof b, "+%s", a);
        } else {
            snprintf(b, sizeof b, "%s", a);
        }
        num(a, sizeof a, s->energy.q[EN_BATTERY_V] == CU_Q_OK ? s->energy.v[EN_BATTERY_V] : NAN, 1, "V");
        snprintf(c, sizeof c, "%s \xc2\xb7 %s", b, a);
        set_text(U.en.sub, c);
    } else {
        set_text(U.en.sub, "");
    }
    /* Chips: charging sources in one chip, then shore power or the 230 V side. */
    int n = 0;
    if (s->energy.connected) {
        float alt = s->energy.v[EN_ALT_W], pv = s->energy.v[EN_PV_W];
        bool has_alt = s->energy.q[EN_ALT_W] == CU_Q_OK && alt > 5, has_pv = s->energy.q[EN_PV_W] == CU_Q_OK && pv > 0;
        if (has_alt || has_pv) {
            c[0] = 0;
            if (has_alt) {
                snprintf(c, sizeof c, "%s %.0f", tr(TX_OV_SRC_ALT), (double)alt);
            }
            if (has_pv) {
                size_t l = strlen(c);
                snprintf(c + l, sizeof c - l, "%s%s %.0f", l ? " \xc2\xb7 " : "", tr(TX_OV_SRC_SOLAR), (double)pv);
            }
            strncat(c, " W", sizeof c - strlen(c) - 1);
            set_chip(&U.en.chip[n++], c, K_OK);
        }
        bool shore = s->energy.q[EN_ACTIVE_INPUT] == CU_Q_OK && s->energy.v[EN_ACTIVE_INPUT] != 240;
        if (shore) {
            snprintf(a, sizeof a, "%.0f V", (double)(s->energy.q[EN_IN_V] == CU_Q_OK ? s->energy.v[EN_IN_V] : 0));
            set_chip(&U.en.chip[n++], fill(c, sizeof c, tr(TX_OV_CHIP_SHORE), "v", a), K_OK);
        } else if (s->energy.q[EN_VEBUS_STATE] == CU_Q_OK) {
            bool inv = s->energy.v[EN_VEBUS_STATE] == 9 || s->energy.v[EN_VEBUS_STATE] == 10;
            set_chip(&U.en.chip[n++], tr(inv ? TX_OV_CHIP_AC_INV : TX_OV_CHIP_AC_OFF), inv ? K_ACCENT : K_NEUTRAL);
        }
    }
    for (int i = 0; i < 2; i++) {
        if (i < n) {
            lv_obj_remove_flag(U.en.chip[i].box, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(U.en.chip[i].box, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void update_overview(const cu_state_t *s)
{
    char a[64], b[96], c[192];
    update_energy_tile(s);

    /* Fridge */
    set_big(U.tiles[1].big, num(a, sizeof a, s->fridge.temp_c, 1, "\xc2\xb0" "C"));
    if (isnan(s->fridge.target_c)) {
        set_text(U.tiles[1].sub, tr(TX_COMMON_UNCALIBRATED));
    } else {
        num(a, sizeof a, s->fridge.target_c, 0, "");
        set_text(U.tiles[1].sub, fill(b, sizeof b, tr(TX_FRIDGE_TARGET_SUB), "t", a));
    }
    const char *ft;
    kind_t fk;
    fridge_status(s, &ft, &fk);
    if (s->fridge.fault_code) {
        snprintf(a, sizeof a, "%u", s->fridge.fault_code);
        ft = fill(c, sizeof c, tr(TX_FRIDGE_FAULT), "n", a);
        fk = K_ERR;
    } else if (!s->fridge.released) {
        ft = tr(TX_COMMON_NOT_RELEASED);
        fk = K_WARN;
    }
    set_chip(&U.tiles[1].chip, ft, fk);

    /* Heater */
    static const tx_t phase[] = {TX_HEAT_PHASE_OFF, TX_HEAT_PHASE_START, TX_HEAT_PHASE_HEATING,
                                 TX_HEAT_PHASE_AFTERRUN, TX_HEAT_PHASE_FAULT};
    static const tx_t chipt[] = {TX_HEAT_CHIP_READY, TX_HEAT_CHIP_WAIT, TX_HEAT_CHIP_HEATING,
                                 TX_HEAT_PHASE_AFTERRUN, TX_HEAT_CHIP_FAULT};
    static const kind_t chipk[] = {K_NEUTRAL, K_ACCENT, K_OK, K_WARN, K_ERR};
    set_big(U.tiles[2].big, tr(phase[s->heater.phase]));
    snprintf(a, sizeof a, "%u", s->heater.level + 1);
    snprintf(b, sizeof b, "%u", s->heater.levels);
    fill(c, sizeof c, tr(TX_HEAT_LEVEL_SUB), "n", a);
    set_text(U.tiles[2].sub, fill(a, sizeof a, c, "max", b));
    if (!s->heater.released) {
        set_chip(&U.tiles[2].chip, tr(TX_COMMON_NOT_RELEASED), K_WARN);
    } else {
        set_chip(&U.tiles[2].chip, tr(chipt[s->heater.phase]), chipk[s->heater.phase]);
    }

    /* Water */
    static const tx_t level[] = {TX_WATER_LEVEL_0, TX_WATER_LEVEL_1, TX_WATER_LEVEL_2, TX_WATER_LEVEL_3};
    const char *lv = s->tanks.fresh_step >= 0 && s->tanks.fresh_step <= 3 ? tr(level[s->tanks.fresh_step]) : "\xe2\x80\x93";
    set_big(U.tiles[3].big, fill(b, sizeof b, tr(TX_WATER_FRESH_SHORT), "level", lv));
    for (int k = 0; k < 3; k++) {
        lv_obj_set_style_bg_color(U.tiles[3].steps[k], lv_color_hex(s->tanks.fresh_step > k ? C_ACCENT : C_SURFACE2),
                                  0);
    }
    if (s->tanks.waste_q != CU_Q_OK) {
        set_chip(&U.tiles[3].chip, tr(TX_COMMON_UNCALIBRATED), K_WARN);
    } else {
        set_chip(&U.tiles[3].chip, tr(s->tanks.waste_full ? TX_WATER_WASTE_FULL_CHIP : TX_WATER_WASTE_OK_CHIP),
                 s->tanks.waste_full ? K_ERR : K_OK);
    }

    /* Roof */
    set_big(U.tiles[4].big, roof_pos_text(s));
    set_text(U.tiles[4].sub, tr(TX_COMMON_TAP));
    const char *rt;
    kind_t rk;
    roof_chip(s, &rt, &rk);
    set_chip(&U.tiles[4].chip, rt, rk);
}

static void update_roof(const cu_state_t *s)
{
    set_text(U.rf_pos, roof_pos_text(s));
    bool running = U.roof_auto && U.roof_dir != ROOF_STOP;
    for (int i = 0; i < 2; i++) {
        if (running) {
            lv_obj_add_flag(U.rf_btn[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(U.rf_btn[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (running) {
        lv_obj_remove_flag(U.rf_stop, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(U.rf_stop, LV_OBJ_FLAG_HIDDEN);
    }
    const char *rt;
    kind_t rk;
    roof_chip(s, &rt, &rk);
    set_chip(&U.rf_chip, rt, rk);
    set_text(U.rf_note, tr(s->roof.tap ? TX_ROOF_NOTE_TAP : TX_ROOF_NOTE));
    for (int i = 0; i < 2; i++) {
        roof_dir_t dir = i == 0 ? ROOF_OPEN : ROOF_CLOSE;
        bool active = U.roof_dir == dir;
        lv_obj_set_style_bg_color(U.rf_btn[i], lv_color_hex(active ? C_ACCENT : C_SURFACE), 0);
        lv_obj_set_style_border_color(U.rf_btn[i], lv_color_hex(active ? C_ACCENT : C_LINE), 0);
        uint32_t ink = active ? C_ACCENT_INK : C_INK;
        lv_obj_set_style_text_color(U.rf_btn_label[i], lv_color_hex(ink), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(U.rf_btn[i], 0), lv_color_hex(ink), 0);
        set_text(U.rf_btn_label[i], tr(i == 0 ? TX_ROOF_OPEN : TX_ROOF_CLOSE));
        set_text(U.rf_btn_hint[i], tr(s->roof.tap ? TX_ROOF_TAP : TX_ROOF_HOLD));
        lv_obj_set_style_text_color(U.rf_btn_hint[i], lv_color_hex(active ? C_ACCENT_INK : C_MUTED), 0);
        bool blocked = !s->roof.released || (dir == ROOF_OPEN && s->vehicle.moving);
        lv_obj_set_style_opa(U.rf_btn[i], blocked && !active ? LV_OPA_40 : LV_OPA_COVER, 0);
    }
}

static void update_fridge(const cu_state_t *s)
{
    char a[64], b[96];
    set_text(U.fr_temp, num(a, sizeof a, s->fridge.temp_c, 1, "\xc2\xb0" "C"));
    const char *ft;
    kind_t fk;
    fridge_status(s, &ft, &fk);
    set_chip(&U.fr_chip, ft, fk);
    set_text(U.fr_rows[0], num(a, sizeof a, s->fridge.raw_v, 2, "V"));
    set_text(U.fr_rows[1], tr(s->fridge.request ? TX_COMMON_YES : TX_COMMON_NO));
    set_text(U.fr_rows[2], s->fridge.drive_known ? tr(s->fridge.output ? TX_COMMON_ON : TX_COMMON_OFF)
                                                 : tr(TX_FRIDGE_OUTPUT_UNCONFIRMED));
    set_text(U.fr_rows[3], tr(TX_FRIDGE_COMPRESSOR_UNKNOWN));
    if (s->fridge.diag_decoding && s->fridge.fault_code) {
        snprintf(a, sizeof a, "%u", s->fridge.fault_code);
        set_text(U.fr_rows[4], fill(b, sizeof b, tr(TX_FRIDGE_DIAG_CODE), "n", a));
    } else {
        snprintf(a, sizeof a, "%lu", (unsigned long)s->fridge.diag_transitions);
        set_text(U.fr_rows[4], fill(b, sizeof b, tr(TX_FRIDGE_DIAG_RAW), "n", a));
    }
    set_text(U.fr_target, isnan(s->fridge.target_c) ? "\xe2\x80\x93" : num(a, sizeof a, s->fridge.target_c, 0, "\xc2\xb0" "C"));
    bool cal = s->fridge.thermo_cal;
    if (cal) {
        lv_obj_remove_state(U.fr_minus, LV_STATE_DISABLED);
        lv_obj_remove_state(U.fr_plus, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(U.fr_minus, LV_STATE_DISABLED);
        lv_obj_add_state(U.fr_plus, LV_STATE_DISABLED);
    }
    set_text(U.fr_toggle_label, tr(s->fridge.on ? TX_FRIDGE_TOGGLE_ON : TX_FRIDGE_TOGGLE_OFF));
    lv_obj_set_style_bg_color(U.fr_toggle, lv_color_hex(s->fridge.on ? C_ACCENT : C_SURFACE2), 0);
    lv_obj_set_style_text_color(U.fr_toggle_label, lv_color_hex(s->fridge.on ? C_ACCENT_INK : C_INK), 0);
}

static void update(lv_timer_t *t)
{
    (void)t;
    state_bus_snapshot(U.cfg.bus, &U.st);
    const cu_state_t *s = &U.st;
    char a[48];
    set_text(U.outside, s->outside_q == CU_Q_OK ? num(a, sizeof a, s->outside_c, 1, "\xc2\xb0" "C") : "\xe2\x80\x93");
    bool unit_ok = s->vehicle.di_ok && s->vehicle.out_ok;
    lv_obj_set_style_bg_color(U.dot_unit, lv_color_hex(unit_ok ? C_OK : C_ERR), 0);
    lv_obj_set_style_bg_color(U.dot_cerbo, lv_color_hex(s->energy.connected ? C_OK : C_ERR), 0);
    uint32_t min = (uint32_t)(s->ts / 60000);
    snprintf(a, sizeof a, "%02lu:%02lu", (unsigned long)((min / 60) % 24), (unsigned long)(min % 60));
    set_text(U.clock, a); /* OPEN: real time from the RTC/SNTP (later milestone); uptime until then */
    update_overview(s);
    update_roof(s);
    update_fridge(s);
    /* Back to the overview after inactivity, never while the roof moves (SPEC 9). */
    if (U.page != UI_PAGE_OVERVIEW && !s->roof.busy && U.roof_dir == ROOF_STOP &&
        lv_display_get_inactive_time(NULL) > (uint32_t)U.cfg.return_min * 60000u) {
        ui_show(UI_PAGE_OVERVIEW);
    }
}

void ui_init(lv_obj_t *parent, const ui_config_t *cfg)
{
    memset(&U, 0, sizeof U);
    U.cfg = *cfg;
    U.root = box(parent, 0, 0, W, H, C_BG, 0);
    lv_obj_set_style_text_color(U.root, lv_color_hex(C_INK), 0);
    build_header();
    U.body = box(U.root, PAD, HEAD + PAD, BODY_W, BODY_H, C_BG, 0);
    for (int i = 0; i < UI_PAGE_COUNT; i++) {
        U.pages[i] = box(U.body, 0, 0, BODY_W, BODY_H, C_BG, 0);
    }
    build_overview(U.pages[UI_PAGE_OVERVIEW]);
    build_roof(U.pages[UI_PAGE_ROOF]);
    build_fridge(U.pages[UI_PAGE_FRIDGE]);
    for (int i = UI_PAGE_ENERGY; i < UI_PAGE_COUNT; i++) {
        build_placeholder(U.pages[i]);
    }
    ui_show(UI_PAGE_OVERVIEW);
    lv_timer_create(roof_timer, 100, NULL);
    lv_timer_create(update, 100, NULL);
    update(NULL);
}
