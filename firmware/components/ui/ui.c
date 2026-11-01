/* LVGL interface after the current web UI ui/reference/web_v1/ (day and night theme).
 * All pages of the web UI: overview, Batterie und Strom (with the MultiPlus switch),
 * Kühlbox, Heizung, Wasser, Dach, Einstellungen, Expertenansicht. */
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

/* Colours of the web UI (style.css #screen), night and day. */
typedef struct {
    uint32_t bg, surface, surface2, line, ink, muted, accent, accent_ink, ok, warn, err;
} pal_t;
static const pal_t NIGHT = {0x0f1419, 0x182029, 0x212b36, 0x2c3845, 0xeef3f7, 0x9aa9b7,
                            0x4fb3d9, 0x06202b, 0x4cc38a, 0xf0b429, 0xef6262};
static const pal_t DAY = {0xeef2f5, 0xffffff, 0xe3e9ee, 0xcfd8df, 0x13212c, 0x51606d,
                          0x0b6e99, 0xffffff, 0x1f8a5a, 0xa86b00, 0xc23b3b};
static const pal_t *PAL = &NIGHT;
#define C_BG (PAL->bg)
#define C_SURFACE (PAL->surface)
#define C_SURFACE2 (PAL->surface2)
#define C_LINE (PAL->line)
#define C_INK (PAL->ink)
#define C_MUTED (PAL->muted)
#define C_ACCENT (PAL->accent)
#define C_ACCENT_INK (PAL->accent_ink)
#define C_OK (PAL->ok)
#define C_WARN (PAL->warn)
#define C_ERR (PAL->err)

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

typedef struct {
    lv_obj_t *batt, *fill, *pct, *bolt;
} batt_t;

typedef enum { THEME_AUTO = 0, THEME_DAY, THEME_NIGHT } theme_t;
typedef enum { X_IO = 0, X_PINS, X_AC, X_CONN, X_NET, X_MEASURE, X_COUNT } xtab_t;

/* Two-tap confirmation (MultiPlus switch, limit, network): armed until the deadline. */
typedef struct {
    int armed;          /* value armed by the first tap, -1 none */
    uint32_t until;
} confirm_t;

#define NET_FIELDS 6
typedef enum { NF_ADDR = 0, NF_GW, NF_DNS, NF_SSID, NF_PSK, NF_HOST } net_field_t;

static struct {
    ui_config_t cfg;
    /* settings (kept over a theme rebuild; stored in LittleFS on the device later) */
    uint8_t brightness;
    theme_t theme;
    bool night;          /* palette in use */
    bool built, timers;
    /* energy page */
    struct {
        batt_t g;
        lv_obj_t *soc_sub, *v, *a, *dc, *in, *shore, *solar, *alt, *starter;
        chip_t mp_chip;
        lv_obj_t *mp_state, *mode_btn[3], *mode_label[3], *mode_note, *inv, *load, *load_bar, *alarm;
        confirm_t mode_confirm;
    } ep;
    /* heater page */
    lv_obj_t *ht_phase, *ht_note, *ht_level, *ht_of, *ht_minus, *ht_plus, *ht_toggle, *ht_toggle_label;
    chip_t ht_chip;
    /* water page */
    lv_obj_t *wa_fresh, *wa_steps[3], *wa_waste;
    chip_t wa_chip;
    /* settings page */
    lv_obj_t *st_bright_val, *st_slider, *st_theme[3], *st_idle[3], *st_lang[2];
    /* expert view */
    xtab_t xtab;
    lv_obj_t *x_tab_btn[X_COUNT], *x_body[X_COUNT];
    lv_obj_t *x_di[CU_N_DI], *x_k[CU_N_RELAYS + CU_N_LADDER], *x_ai[CU_N_AI], *x_rail;
    lv_obj_t *x_pin_btn[32], *x_pin_name, *x_pin_desc;
    uint8_t x_pin;
    lv_obj_t *x_ac_in[4], *x_ac_out[5], *x_ac_dev[7], *x_lim, *x_lim_set, *x_lim_set_label, *x_ctl_note;
    float x_lim_value;
    confirm_t lim_confirm;
    lv_obj_t *x_conn[5], *x_unit[4];
    lv_obj_t *x_eth_mode[2], *x_net_val[NET_FIELDS], *x_net_apply, *x_net_apply_label, *x_net_note;
    bool net_static;
    char net[NET_FIELDS][64];
    confirm_t net_confirm;
    lv_obj_t *kb_layer, *kb_ta, *kb;
    net_field_t kb_field;
    lv_obj_t *root, *body;
    lv_obj_t *back, *brand, *title, *outside, *dot_unit, *dot_cerbo, *clock;
    lv_obj_t *pages[UI_PAGE_COUNT];
    ui_page_t page;
    tile_t tiles[5];    /* fridge, heat, water, roof (index 0 unused: energy tile below) */
    struct {
        batt_t g;
        lv_obj_t *status, *sub, *chips;
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
    if (dot && i18n_lang() == LANG_DE) {
        *dot = ','; /* decimal comma in German, point in English (as the web UI) */
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

static const tx_t page_title[UI_PAGE_COUNT] = {TX_PAGE_OVERVIEW, TX_PAGE_ROOF,  TX_PAGE_FRIDGE,   TX_PAGE_ENERGY,
                                               TX_PAGE_HEAT,     TX_PAGE_WATER, TX_PAGE_SETTINGS, TX_PAGE_EXPERT};

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
#define BATT_W 248
static void build_battery(lv_obj_t *parent, batt_t *g)
{
    lv_obj_t *wrap = box(parent, 0, 0, BATT_W + 16, 104, C_SURFACE, 0);
    lv_obj_set_style_bg_opa(wrap, LV_OPA_TRANSP, 0);
    g->batt = box(wrap, 0, 0, BATT_W, 104, C_SURFACE, 18);
    border(g->batt, C_INK, 5);
    lv_obj_set_style_pad_all(g->batt, 6, 0);
    box(wrap, BATT_W + 5, 32, 10, 40, C_INK, 6); /* terminal */
    g->fill = box(g->batt, 0, 0, 0, LV_PCT(100), C_OK, 10);
    lv_obj_t *lab = box(g->batt, 0, 0, LV_PCT(100), LV_PCT(100), C_SURFACE, 0);
    lv_obj_set_style_bg_opa(lab, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(lab, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(lab, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(lab, 4, 0);
    g->bolt = text(lab, &lv_font_montserrat_24, C_INK, LV_SYMBOL_CHARGE);
    g->pct = text(lab, &font_44_bold, C_INK, "\xe2\x80\x93");
}

static void update_battery(batt_t *g, const cu_state_t *s, bool charging)
{
    char a[32];
    bool soc_ok = s->energy.q[EN_SOC] == CU_Q_OK;
    float soc = soc_ok ? s->energy.v[EN_SOC] : NAN;
    set_text(g->pct, soc_ok ? (snprintf(a, sizeof a, "%.0f %%", (double)soc), a) : "\xe2\x80\x93");
    int32_t inner = BATT_W - 2 * 5 - 2 * 6;
    lv_obj_set_width(g->fill, soc_ok ? (int32_t)(inner * (soc < 0 ? 0 : soc > 100 ? 100 : soc) / 100) : 0);
    lv_obj_set_style_bg_color(g->fill, lv_color_hex(soc < 15 ? C_ERR : soc < 30 ? C_WARN : C_OK), 0);
    if (charging) {
        lv_obj_remove_flag(g->bolt, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(g->bolt, LV_OBJ_FLAG_HIDDEN);
    }
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
    build_battery(body, &U.en.g);
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
    bool bw_ok = s->energy.connected && s->energy.q[EN_BATTERY_W] == CU_Q_OK;
    bool charging = false;
    if (!s->energy.connected) {
        set_text(U.en.status, tr(TX_COMMON_NO_VICTRON));
    } else if (!bw_ok) {
        set_text(U.en.status, tr(TX_COMMON_NO_DATA));
    } else {
        set_text(U.en.status, tr(battery_status(s, &charging)));
    }
    update_battery(&U.en.g, s, charging);
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

/* ---------- shared building blocks for the M2 pages ---------- */

static lv_obj_t *row_f(lv_obj_t *parent, tx_t name, const lv_font_t *font)
{
    lv_obj_t *r = box(parent, 0, 0, LV_PCT(100), LV_SIZE_CONTENT, C_SURFACE, 0);
    lv_obj_set_style_border_side(r, LV_BORDER_SIDE_TOP, 0);
    border(r, C_LINE, 1);
    lv_obj_set_style_pad_ver(r, font == &font_18 ? 9 : 6, 0);
    lv_obj_t *n = text(r, font, C_MUTED, tr(name));
    lv_obj_align(n, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *v = text(r, font, C_INK, "\xe2\x80\x93");
    lv_obj_align(v, LV_ALIGN_RIGHT_MID, 0, 0);
    return v;
}

static void transparent(lv_obj_t *o)
{
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
}

static lv_obj_t *seg(lv_obj_t *parent, int n, const tx_t *labels, lv_event_cb_t cb, lv_obj_t **btns, lv_obj_t **lbls,
                     int32_t h)
{
    lv_obj_t *s = box(parent, 0, 0, LV_PCT(100), h + 12, C_SURFACE2, 16);
    lv_obj_set_style_pad_all(s, 6, 0);
    lv_obj_set_flex_flow(s, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(s, 8, 0);
    for (int i = 0; i < n; i++) {
        lv_obj_t *b = box(s, 0, 0, 0, h, C_SURFACE2, 12);
        transparent(b);
        lv_obj_set_flex_grow(b, 1);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *l = text(b, h >= 60 ? &font_20_semi : &font_16_semi, C_INK, tr(labels[i]));
        lv_obj_center(l);
        btns[i] = b;
        if (lbls) {
            lbls[i] = l;
        }
    }
    return s;
}

static void seg_set(lv_obj_t **btns, int n, int active)
{
    for (int i = 0; i < n; i++) {
        bool on = i == active;
        lv_obj_set_style_bg_opa(btns[i], on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_color(btns[i], lv_color_hex(C_ACCENT), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(btns[i], 0), lv_color_hex(on ? C_ACCENT_INK : C_INK), 0);
    }
}

static void enable(lv_obj_t *o, bool on)
{
    if (on) {
        lv_obj_remove_state(o, LV_STATE_DISABLED);
        lv_obj_set_style_opa(o, LV_OPA_COVER, 0);
    } else {
        lv_obj_add_state(o, LV_STATE_DISABLED);
        lv_obj_set_style_opa(o, LV_OPA_40, 0);
    }
}

static lv_obj_t *note(lv_obj_t *parent, const char *s)
{
    lv_obj_t *l = text(parent, &font_15_semi, C_MUTED, s);
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(l, 4, 0);
    return l;
}

static lv_obj_t *big_btn(lv_obj_t *parent, int32_t h, lv_event_cb_t cb, void *ud, lv_obj_t **label)
{
    lv_obj_t *b = box(parent, 0, 0, LV_PCT(100), h, C_SURFACE2, 16);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(b, lv_color_hex(C_LINE), LV_STATE_PRESSED);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    *label = text(b, h >= 80 ? &font_22_semi : &font_18, C_INK, "");
    lv_obj_center(*label);
    return b;
}

/* First tap arms, second tap within 3 s confirms. */
static bool confirm_tap(confirm_t *c, int value)
{
    if (c->armed == value && lv_tick_get() < c->until) {
        c->armed = -1;
        return true;
    }
    c->armed = value;
    c->until = lv_tick_get() + 3000;
    return false;
}

static bool armed(const confirm_t *c, int value)
{
    return c->armed == value && lv_tick_get() < c->until;
}

static const char *signed_num(char *buf, size_t len, float v, int d, const char *unit)
{
    char t[40];
    num(t, sizeof t, v, d, unit);
    snprintf(buf, len, "%s%s", (!isnan(v) && v > 0) ? "+" : "", t);
    return buf;
}

static float ev(const cu_state_t *s, energy_key_t k)
{
    return s->energy.q[k] == CU_Q_OK ? s->energy.v[k] : NAN;
}

/* ---------- Batterie und Strom ---------- */

static const uint8_t MODES[3] = {1, 3, 4};

static void mode_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (!U.st.victron.enabled) {
        return;
    }
    if (confirm_tap(&U.ep.mode_confirm, MODES[i])) {
        cu_cmd_t c = {.type = CMD_VICTRON_MODE, .value = MODES[i]};
        send(&c);
    }
}

static void build_energy(lv_obj_t *page)
{
    int32_t pw = (BODY_W - 2 * GAP) / 3;
    U.ep.mode_confirm.armed = -1;
    lv_obj_t *p = panel(page, 0, pw);
    label_caps(p, TX_TILE_BATTERY);
    lv_obj_t *bw = box(p, 0, 0, LV_PCT(100), 104, C_SURFACE, 0);
    lv_obj_set_flex_flow(bw, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bw, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    build_battery(bw, &U.ep.g);
    U.ep.soc_sub = text(p, &font_18, C_MUTED, "");
    lv_obj_set_width(U.ep.soc_sub, LV_PCT(100));
    lv_obj_set_style_text_align(U.ep.soc_sub, LV_TEXT_ALIGN_CENTER, 0);
    filler(p);
    U.ep.v = row_f(p, TX_BAT_VOLTAGE, &font_18);
    U.ep.a = row_f(p, TX_BAT_CURRENT, &font_18);
    U.ep.dc = row_f(p, TX_EN_DC, &font_18);

    lv_obj_t *q = panel(page, pw + GAP, pw);
    lv_obj_t *hl = box(q, 0, 0, LV_PCT(100), LV_SIZE_CONTENT, C_SURFACE, 0);
    lv_obj_t *l = label_caps(hl, TX_EN_IN);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
    U.ep.in = text(hl, &font_46_semi, C_INK, "\xe2\x80\x93");
    lv_obj_align(U.ep.in, LV_ALIGN_RIGHT_MID, 0, 0);
    filler(q);
    U.ep.shore = row_f(q, TX_EN_SRC_SHORE, &font_18);
    U.ep.solar = row_f(q, TX_EN_SRC_SOLAR, &font_18);
    U.ep.alt = row_f(q, TX_EN_SRC_ALT, &font_18);
    U.ep.starter = row_f(q, TX_EN_STARTER, &font_18);

    lv_obj_t *m = panel(page, 2 * (pw + GAP), BODY_W - 2 * (pw + GAP));
    lv_obj_set_style_pad_row(m, 8, 0);
    lv_obj_t *mh = box(m, 0, 0, LV_PCT(100), 34, C_SURFACE, 0);
    l = label_caps(mh, TX_EN_MP);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
    U.ep.mp_chip = chip(mh);
    lv_obj_align(U.ep.mp_chip.box, LV_ALIGN_RIGHT_MID, 0, 0);
    U.ep.mp_state = text(m, &font_18, C_MUTED, "");
    static const tx_t ml[3] = {TX_X_MODE_1, TX_X_MODE_3, TX_X_MODE_4};
    seg(m, 3, ml, mode_cb, U.ep.mode_btn, U.ep.mode_label, 52);
    U.ep.mode_note = note(m, "");
    filler(m);
    U.ep.inv = row_f(m, TX_EN_INV, &font_16_semi);
    U.ep.load = row_f(m, TX_EN_LOAD, &font_16_semi);
    U.ep.load_bar = lv_bar_create(m);
    lv_obj_set_size(U.ep.load_bar, LV_PCT(100), 12);
    lv_bar_set_range(U.ep.load_bar, 0, 100);
    lv_obj_set_style_bg_color(U.ep.load_bar, lv_color_hex(C_SURFACE2), LV_PART_MAIN);
    lv_obj_set_style_bg_color(U.ep.load_bar, lv_color_hex(C_ACCENT), LV_PART_INDICATOR);
    U.ep.alarm = row_f(m, TX_EN_ALARMS, &font_16_semi);
}

/* Phase name and power of one charging device; "aus" when it delivers nothing. */
static const char *source(char *buf, size_t len, float phase, float w)
{
    char t[32];
    if (isnan(phase) && isnan(w)) {
        snprintf(buf, len, "\xe2\x80\x93");
        return buf;
    }
    int ph = isnan(phase) ? -1 : (int)phase;
    if (ph == 2) {
        snprintf(buf, len, "%s", tr(TX_PHASE_2));
        return buf;
    }
    bool watts = !isnan(w) && w > 5;
    static const tx_t names[] = {TX_PHASE_3, TX_PHASE_4, TX_PHASE_5, TX_PHASE_6, TX_PHASE_7};
    const char *name = (ph >= 3 && ph <= 7) ? tr(names[ph - 3]) : watts ? tr(TX_PHASE_ON) : tr(TX_PHASE_0);
    if (watts) {
        snprintf(buf, len, "%s \xc2\xb7 %s", name, num(t, sizeof t, w, 0, "W"));
    } else {
        snprintf(buf, len, "%s", name);
    }
    return buf;
}

static void update_energy(const cu_state_t *s)
{
    char a[96], b[64];
    bool charging = false;
    bool has = s->energy.connected;
    bool bw_ok = has && s->energy.q[EN_BATTERY_W] == CU_Q_OK;
    tx_t status = bw_ok ? battery_status(s, &charging) : (has ? TX_COMMON_NO_DATA : TX_COMMON_NO_VICTRON);
    update_battery(&U.ep.g, s, charging);
    set_text(U.ep.soc_sub, tr(status));
    set_text(U.ep.v, num(a, sizeof a, ev(s, EN_BATTERY_V), 1, "V"));
    set_text(U.ep.a, signed_num(a, sizeof a, ev(s, EN_BATTERY_A), 1, "A"));
    set_text(U.ep.dc, num(a, sizeof a, ev(s, EN_DC_SYSTEM_W), 0, "W"));

    float state = ev(s, EN_VEBUS_STATE), active = ev(s, EN_ACTIVE_INPUT), mode = ev(s, EN_VEBUS_MODE);
    bool shore = !isnan(active) && active != 240;
    float vdc = ev(s, EN_VEBUS_DC_W), pv = ev(s, EN_PV_W), alt = ev(s, EN_ALT_W);
    float shore_w = shore && !isnan(vdc) && vdc > 0 ? vdc : 0;
    float total = shore_w + (pv > 0 ? pv : 0) + (alt > 0 ? alt : 0);
    set_text(U.ep.in, !bw_ok ? "\xe2\x80\x93" : total > 5 ? num(a, sizeof a, total, 0, "W") : tr(TX_EN_CHARGING_OFF));
    set_text(U.ep.shore, isnan(state) ? "\xe2\x80\x93" : shore ? source(a, sizeof a, state, shore_w) : tr(TX_EN_NO_SHORE));
    set_text(U.ep.solar, source(a, sizeof a, ev(s, EN_SOLAR_STATE), pv));
    set_text(U.ep.alt, source(a, sizeof a, ev(s, EN_ALT_STATE), alt));
    set_text(U.ep.starter, num(a, sizeof a, ev(s, EN_STARTER_V), 1, "V"));

    /* MultiPlus: chip, what it does, switch, 230 V side */
    float overload = ev(s, EN_ALARM_OVERLOAD);
    bool charger_only = mode == 1 && !shore;
    int st = isnan(state) ? -1 : (int)state;
    if (st < 0) {
        set_chip(&U.ep.mp_chip, tr(TX_COMMON_NO_DATA), K_WARN);
        set_text(U.ep.mp_state, has ? "\xe2\x80\x93" : tr(TX_COMMON_NO_VICTRON));
    } else {
        tx_t chipt = overload == 2 ? TX_EN_ALARM_OVERLOAD : charger_only ? TX_MP_CHIP_0 : TX_COMMON_NO_DATA;
        static const tx_t mp_chip[12] = {TX_MP_CHIP_0, TX_MP_CHIP_1, TX_MP_CHIP_2,  TX_MP_CHIP_3,
                                         TX_MP_CHIP_4, TX_MP_CHIP_5, TX_MP_CHIP_6,  TX_MP_CHIP_7,
                                         TX_MP_CHIP_8, TX_MP_CHIP_9, TX_MP_CHIP_10, TX_MP_CHIP_11};
        static const tx_t desc[12] = {TX_VEBUS_DESC_0, TX_VEBUS_DESC_1, TX_VEBUS_DESC_2,  TX_VEBUS_DESC_3,
                                      TX_VEBUS_DESC_4, TX_VEBUS_DESC_5, TX_VEBUS_DESC_6,  TX_VEBUS_DESC_7,
                                      TX_VEBUS_DESC_8, TX_VEBUS_DESC_9, TX_VEBUS_DESC_10, TX_VEBUS_DESC_11};
        if (overload != 2 && !charger_only && st <= 11) {
            chipt = mp_chip[st];
        }
        set_chip(&U.ep.mp_chip, tr(chipt), overload == 2 || st == 2 ? K_ERR : (st == 0 || charger_only) ? K_NEUTRAL : K_OK);
        if (mode == 1) {
            set_text(U.ep.mp_state, shore && st <= 11 ? fill(a, sizeof a, tr(TX_MP_CHARGER_ONLY_SHORE), "desc", tr(desc[st]))
                                                      : tr(TX_MP_CHARGER_ONLY));
        } else {
            set_text(U.ep.mp_state, st <= 11 ? tr(desc[st]) : "\xe2\x80\x93");
        }
    }
    int active_i = mode == 1 ? 0 : mode == 3 ? 1 : mode == 4 ? 2 : -1;
    seg_set(U.ep.mode_btn, 3, active_i);
    static const tx_t ml[3] = {TX_X_MODE_1, TX_X_MODE_3, TX_X_MODE_4};
    for (int i = 0; i < 3; i++) {
        set_text(U.ep.mode_label[i], armed(&U.ep.mode_confirm, MODES[i]) ? tr(TX_X_CONFIRM) : tr(ml[i]));
        enable(U.ep.mode_btn[i], s->victron.enabled && has && (s->victron.modes & (1u << MODES[i])));
    }
    tx_t n = !s->victron.enabled ? TX_MP_MODE_DISABLED
             : !has ? TX_X_CTL_OFFLINE
             : s->victron.pending_mode ? TX_X_CTL_PENDING
             : s->victron.unconfirmed == VW_MODE ? TX_X_CTL_UNCONFIRMED
             : s->victron.last == VC_TOO_FAST ? TX_X_VC_TOO_FAST
             : TX_MP_MODE_HINT;
    set_text(U.ep.mode_note, tr(n));
    float outp = ev(s, EN_OUT_P), outs = ev(s, EN_OUT_S);
    set_text(U.ep.inv, num(a, sizeof a, st == 0 ? 0 : outp, 0, "W"));
    float load = isnan(outs) ? NAN : outs / 1600.0f * 100.0f; /* MultiPlus Compact 12/1600 */
    if (isnan(load)) {
        set_text(U.ep.load, "\xe2\x80\x93");
    } else {
        snprintf(a, sizeof a, "%.0f %% \xc2\xb7 %s", (double)load, num(b, sizeof b, outs, 0, "VA"));
        set_text(U.ep.load, a);
    }
    lv_bar_set_value(U.ep.load_bar, overload == 2 ? 100 : isnan(load) ? 0 : (int32_t)load, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(U.ep.load_bar, lv_color_hex(overload == 2 ? C_ERR : load > 80 ? C_WARN : C_ACCENT),
                              LV_PART_INDICATOR);
    set_text(U.ep.alarm, isnan(overload) ? "\xe2\x80\x93" : overload == 2 ? tr(TX_EN_ALARM_OVERLOAD) : tr(TX_EN_ALARM_NONE));
}

/* ---------- Heizung ---------- */

static void heat_cb(lv_event_t *e)
{
    int what = (int)(intptr_t)lv_event_get_user_data(e);
    const cu_state_t *s = &U.st;
    cu_cmd_t c = {.type = CMD_HEATER, .on = s->heater.on, .value = s->heater.level};
    if (what == 0) {
        c.on = !s->heater.on;
    } else {
        c.value = (float)(s->heater.level + what);
    }
    send(&c);
}

static lv_obj_t *step_cb_btn(lv_obj_t *parent, const char *sym, lv_event_cb_t cb, int delta, int32_t size)
{
    lv_obj_t *b = box(parent, 0, 0, size, size, C_SURFACE2, size >= 100 ? 20 : 14);
    lv_obj_t *l = text(b, size >= 100 ? &font_46_semi : &font_28_semi, C_INK, sym);
    lv_obj_center(l);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(b, lv_color_hex(C_LINE), LV_STATE_PRESSED);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)(intptr_t)delta);
    return b;
}

static void build_heat(lv_obj_t *page)
{
    int32_t lw = (BODY_W - GAP) * 4 / 9, rw = BODY_W - GAP - lw;
    lv_obj_t *p = panel(page, 0, lw);
    label_caps(p, TX_HEAT_STATE);
    U.ht_phase = text(p, &font_56_semi, C_INK, "\xe2\x80\x93");
    U.ht_chip = chip(p);
    filler(p);
    U.ht_note = note(p, "");

    lv_obj_t *q = panel(page, lw + GAP, rw);
    label_caps(q, TX_HEAT_LEVEL);
    lv_obj_t *stepper = box(q, 0, 0, LV_PCT(100), 112, C_SURFACE, 0);
    U.ht_minus = step_cb_btn(stepper, "\xe2\x88\x92", heat_cb, -1, 112);
    lv_obj_align(U.ht_minus, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *val = box(stepper, 0, 0, LV_SIZE_CONTENT, LV_SIZE_CONTENT, C_SURFACE, 0);
    lv_obj_set_flex_flow(val, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(val, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(val, LV_ALIGN_CENTER, 0, 0);
    U.ht_level = text(val, &font_84_semi, C_INK, "\xe2\x80\x93");
    U.ht_of = text(val, &font_18, C_MUTED, "");
    U.ht_plus = step_cb_btn(stepper, "+", heat_cb, 1, 112);
    lv_obj_align(U.ht_plus, LV_ALIGN_RIGHT_MID, 0, 0);
    U.ht_toggle = big_btn(q, 88, heat_cb, (void *)(intptr_t)0, &U.ht_toggle_label);
}

static void update_heat(const cu_state_t *s)
{
    char a[48], b[48];
    static const tx_t phase[] = {TX_HEAT_PHASE_OFF, TX_HEAT_PHASE_START, TX_HEAT_PHASE_HEATING,
                                 TX_HEAT_PHASE_AFTERRUN, TX_HEAT_PHASE_FAULT};
    static const tx_t chipt[] = {TX_HEAT_CHIP_READY, TX_HEAT_CHIP_WAIT, TX_HEAT_CHIP_HEATING,
                                 TX_HEAT_PHASE_AFTERRUN, TX_HEAT_CHIP_FAULT};
    static const kind_t chipk[] = {K_NEUTRAL, K_ACCENT, K_OK, K_WARN, K_ERR};
    set_text(U.ht_phase, tr(phase[s->heater.phase]));
    if (!s->heater.released) {
        set_chip(&U.ht_chip, tr(TX_COMMON_NOT_RELEASED), K_WARN);
    } else {
        set_chip(&U.ht_chip, tr(chipt[s->heater.phase]), chipk[s->heater.phase]);
    }
    set_text(U.ht_note, tr(s->heater.phase == HEAT_AFTERRUN ? TX_HEAT_NOTE_AFTERRUN
                           : s->heater.phase == HEAT_FAULT  ? TX_HEAT_NOTE_FAULT
                                                            : TX_HEAT_NOTE));
    snprintf(a, sizeof a, "%u", s->heater.level + 1);
    set_text(U.ht_level, a);
    snprintf(b, sizeof b, "%u", s->heater.levels);
    set_text(U.ht_of, s->heater.level_cal ? fill(a, sizeof a, tr(TX_HEAT_OF), "n", b) : tr(TX_HEAT_LEVEL_UNCAL));
    bool rel = s->heater.released;
    enable(U.ht_minus, rel && s->heater.level_cal && s->heater.level > 0);
    enable(U.ht_plus, rel && s->heater.level_cal && s->heater.level + 1 < s->heater.levels);
    enable(U.ht_toggle, rel);
    set_text(U.ht_toggle_label, tr(s->heater.on ? TX_HEAT_TOGGLE_ON : TX_HEAT_TOGGLE_OFF));
    lv_obj_set_style_bg_color(U.ht_toggle, lv_color_hex(s->heater.on ? C_ACCENT : C_SURFACE2), 0);
    lv_obj_set_style_text_color(U.ht_toggle_label, lv_color_hex(s->heater.on ? C_ACCENT_INK : C_INK), 0);
}

/* ---------- Wasser ---------- */

static void build_water(lv_obj_t *page)
{
    int32_t lw = (BODY_W - GAP) * 14 / 24, rw = BODY_W - GAP - lw;
    lv_obj_t *p = panel(page, 0, lw);
    lv_obj_set_style_pad_row(p, 14, 0);
    label_caps(p, TX_WATER_FRESH);
    U.wa_fresh = text(p, &font_56_semi, C_INK, "\xe2\x80\x93");
    lv_obj_t *r = box(p, 0, 0, LV_PCT(100), 72, C_SURFACE, 0);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(r, 10, 0);
    for (int k = 0; k < 3; k++) {
        U.wa_steps[k] = box(r, 0, 0, 0, 72, C_SURFACE2, 12);
        lv_obj_set_flex_grow(U.wa_steps[k], 1);
    }
    filler(p);
    note(p, tr(TX_WATER_NOTE_FRESH));

    lv_obj_t *q = panel(page, lw + GAP, rw);
    lv_obj_set_style_pad_row(q, 14, 0);
    label_caps(q, TX_WATER_WASTE);
    U.wa_waste = text(q, &font_56_semi, C_INK, "\xe2\x80\x93");
    U.wa_chip = chip(q);
    filler(q);
    note(q, tr(TX_WATER_NOTE_WASTE));
}

static void update_water(const cu_state_t *s)
{
    static const tx_t level[] = {TX_WATER_LEVEL_0, TX_WATER_LEVEL_1, TX_WATER_LEVEL_2, TX_WATER_LEVEL_3};
    int8_t st = s->tanks.fresh_step;
    set_text(U.wa_fresh, st >= 0 && st <= 3 ? tr(level[st]) : tr(TX_COMMON_UNCALIBRATED));
    for (int k = 0; k < 3; k++) {
        lv_obj_set_style_bg_color(U.wa_steps[k], lv_color_hex(st > k ? C_ACCENT : C_SURFACE2), 0);
    }
    if (s->tanks.waste_q != CU_Q_OK) {
        set_text(U.wa_waste, "\xe2\x80\x93");
        set_chip(&U.wa_chip, tr(TX_COMMON_UNCALIBRATED), K_WARN);
    } else {
        set_text(U.wa_waste, tr(s->tanks.waste_full ? TX_WATER_WASTE_FULL : TX_WATER_WASTE_OK));
        set_chip(&U.wa_chip, tr(s->tanks.waste_full ? TX_WATER_EMPTY_IT : TX_WATER_IN_ORDER),
                 s->tanks.waste_full ? K_ERR : K_OK);
    }
}

/* ---------- Einstellungen ---------- */

static void rebuild(void);

static void apply_theme(void)
{
    bool night = U.theme == THEME_NIGHT || (U.theme == THEME_AUTO && U.st.vehicle.dim58);
    if (night != U.night) {
        U.night = night;
        PAL = night ? &NIGHT : &DAY;
        rebuild();
    }
}

static void theme_cb(lv_event_t *e)
{
    U.theme = (theme_t)(intptr_t)lv_event_get_user_data(e);
    seg_set(U.st_theme, 3, U.theme);
    apply_theme();
}

static void idle_cb(lv_event_t *e)
{
    static const uint8_t mins[3] = {1, 2, 5};
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    U.cfg.return_min = mins[i];
    seg_set(U.st_idle, 3, i);
}

static void lang_cb(lv_event_t *e)
{
    lang_t l = (lang_t)(intptr_t)lv_event_get_user_data(e);
    if (l != i18n_lang()) {
        i18n_set_lang(l); /* TODO(M4): store in LittleFS */
        rebuild();        /* every text is set when the pages are built */
    }
}

static void bright_cb(lv_event_t *e)
{
    U.brightness = (uint8_t)lv_slider_get_value(lv_event_get_target(e));
    char a[16];
    snprintf(a, sizeof a, "%u %%", U.brightness);
    set_text(U.st_bright_val, a); /* TODO(O15): backlight PWM on the device; on/off only until confirmed */
}

static void build_settings(lv_obj_t *page)
{
    int32_t pw = (BODY_W - GAP) / 2;
    lv_obj_t *p = panel(page, 0, pw);
    label_caps(p, TX_SET_SCREEN);
    lv_obj_t *hl = box(p, 0, 0, LV_PCT(100), LV_SIZE_CONTENT, C_SURFACE, 0);
    lv_obj_t *l = text(hl, &font_18, C_MUTED, tr(TX_SET_BRIGHTNESS));
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
    U.st_bright_val = text(hl, &font_46_semi, C_INK, "");
    lv_obj_align(U.st_bright_val, LV_ALIGN_RIGHT_MID, 0, 0);
    U.st_slider = lv_slider_create(p);
    lv_obj_set_width(U.st_slider, LV_PCT(96));
    lv_obj_set_style_margin_hor(U.st_slider, 12, 0);
    lv_slider_set_range(U.st_slider, 10, 100);
    lv_slider_set_value(U.st_slider, U.brightness, LV_ANIM_OFF);
    lv_obj_set_height(U.st_slider, 14);
    lv_obj_set_style_bg_color(U.st_slider, lv_color_hex(C_SURFACE2), LV_PART_MAIN);
    lv_obj_set_style_bg_color(U.st_slider, lv_color_hex(C_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(U.st_slider, lv_color_hex(C_ACCENT), LV_PART_KNOB);
    lv_obj_set_style_pad_all(U.st_slider, 19, LV_PART_KNOB);
    lv_obj_add_event_cb(U.st_slider, bright_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t *sp = box(p, 0, 0, 1, 8, C_SURFACE, 0);
    transparent(sp);
    text(p, &font_18, C_MUTED, tr(TX_SET_APPEARANCE));
    static const tx_t th[3] = {TX_SET_THEME_AUTO, TX_SET_THEME_LIGHT, TX_SET_THEME_DARK};
    seg(p, 3, th, theme_cb, U.st_theme, NULL, 52);
    seg_set(U.st_theme, 3, U.theme);
    filler(p);
    char nb[256];
    snprintf(nb, sizeof nb, "%s %s", tr(TX_SET_THEME_NOTE), tr(TX_SET_BRIGHTNESS_NA));
    note(p, nb);
    char a[16];
    snprintf(a, sizeof a, "%u %%", U.brightness);
    set_text(U.st_bright_val, a);

    lv_obj_t *q = panel(page, pw + GAP, BODY_W - pw - GAP);
    label_caps(q, TX_SET_OPERATION);
    text(q, &font_18, C_MUTED, tr(TX_SET_IDLE));
    static const tx_t il[3] = {TX_SET_IDLE_1, TX_SET_IDLE_2, TX_SET_IDLE_5};
    seg(q, 3, il, idle_cb, U.st_idle, NULL, 52);
    seg_set(U.st_idle, 3, U.cfg.return_min == 1 ? 0 : U.cfg.return_min == 5 ? 2 : 1);
    lv_obj_t *sp2 = box(q, 0, 0, 1, 6, C_SURFACE, 0);
    transparent(sp2);
    text(q, &font_18, C_MUTED, tr(TX_SET_LANGUAGE));
    static const tx_t ll[2] = {TX_SET_LANG_DE, TX_SET_LANG_EN};
    seg(q, 2, ll, lang_cb, U.st_lang, NULL, 52);
    seg_set(U.st_lang, 2, i18n_lang() == LANG_EN ? 1 : 0);
    filler(q);
    lv_obj_t *lab;
    lv_obj_t *b = big_btn(q, 76, go_cb, (void *)(intptr_t)UI_PAGE_EXPERT, &lab);
    set_text(lab, tr(TX_SET_EXPERT));
    lv_obj_align(lab, LV_ALIGN_LEFT_MID, 24, 0);
    lv_obj_t *chev = text(b, &lv_font_montserrat_24, C_INK, LV_SYMBOL_RIGHT);
    lv_obj_align(chev, LV_ALIGN_RIGHT_MID, -24, 0);
}

/* ---------- Expertenansicht ---------- */

#define XBODY_Y 76
#define XBODY_H (BODY_H - XBODY_Y)

void ui_mp_tap(uint8_t mode)
{
    for (int i = 0; i < 3; i++) {
        if (MODES[i] == mode) {
            lv_obj_send_event(U.ep.mode_btn[i], LV_EVENT_CLICKED, NULL);
        }
    }
}

void ui_expert_tab(int tab)
{
    if (tab < 0 || tab >= X_COUNT) {
        return;
    }
    U.xtab = (xtab_t)tab;
    seg_set(U.x_tab_btn, X_COUNT, tab);
    for (int i = 0; i < X_COUNT; i++) {
        if (i == tab) {
            lv_obj_remove_flag(U.x_body[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(U.x_body[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void xtab_cb(lv_event_t *e)
{
    ui_expert_tab((int)(intptr_t)lv_event_get_user_data(e));
}

static lv_obj_t *xpanel(lv_obj_t *parent, int32_t x, int32_t w, tx_t label)
{
    lv_obj_t *p = box(parent, x, 0, w, XBODY_H, C_SURFACE, 16);
    border(p, C_LINE, 1);
    lv_obj_set_style_pad_hor(p, 16, 0);
    lv_obj_set_style_pad_ver(p, 14, 0);
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(p, 6, 0);
    label_caps(p, label);
    return p;
}

static lv_obj_t *xlist(lv_obj_t *panel_obj)
{
    lv_obj_t *l = box(panel_obj, 0, 0, LV_PCT(100), 0, C_SURFACE, 0);
    lv_obj_set_flex_grow(l, 1);
    lv_obj_set_flex_flow(l, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(l, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(l, LV_DIR_VER);
    return l;
}

/* "DI1  Dach Rückmeldung A · T32b/15 ........ 1" */
static lv_obj_t *io_row(lv_obj_t *list, const char *ch, const char *name, bool free_ch)
{
    lv_obj_t *r = box(list, 0, 0, LV_PCT(100), 30, C_SURFACE, 0);
    lv_obj_set_style_border_side(r, LV_BORDER_SIDE_TOP, 0);
    border(r, C_LINE, 1);
    lv_obj_t *c = text(r, &font_15_semi, C_MUTED, ch);
    lv_obj_align(c, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *n = text(r, &font_15_semi, C_INK, name);
    lv_obj_set_size(n, 250, 20);
    lv_label_set_long_mode(n, LV_LABEL_LONG_DOT);
    lv_obj_align(n, LV_ALIGN_LEFT_MID, 50, 0);
    lv_obj_t *v = text(r, &font_15_semi, C_INK, "\xe2\x80\x93");
    lv_obj_align(v, LV_ALIGN_RIGHT_MID, 0, 0);
    if (free_ch) {
        lv_obj_set_style_opa(r, LV_OPA_50, 0);
    }
    return v;
}

static void build_x_io(lv_obj_t *body)
{
    int32_t w = (BODY_W - 2 * GAP) / 3;
    char ch[8];
    lv_obj_t *l = xlist(xpanel(body, 0, w, TX_X_DI));
    for (int i = 0; i < CU_N_DI; i++) {
        snprintf(ch, sizeof ch, "DI%d", i + 1);
        tx_t name = i < TXL_IO_DI_LEN ? TXL_IO_DI[i] : TX_NONE;
        U.x_di[i] = io_row(l, ch, name == TX_NONE ? tr(TX_X_FREE) : tr(name), name == TX_NONE);
    }
    l = xlist(xpanel(body, w + GAP, w, TX_X_DO));
    for (int i = 0; i < CU_N_RELAYS; i++) {
        snprintf(ch, sizeof ch, "K%d", i + 1);
        tx_t name = i < TXL_IO_K_LEN ? TXL_IO_K[i] : TX_NONE;
        U.x_k[i] = io_row(l, ch, name == TX_NONE ? tr(TX_X_UNUSED_K) : tr(name), name == TX_NONE);
    }
    for (int i = 0; i < CU_N_LADDER; i++) {
        char nm[64], n[4];
        snprintf(ch, sizeof ch, "H%d", i + 1);
        snprintf(n, sizeof n, "%d", i + 1);
        U.x_k[CU_N_RELAYS + i] = io_row(l, ch, fill(nm, sizeof nm, tr(TX_X_LADDER_SEG), "n", n), false);
    }
    l = xlist(xpanel(body, 2 * (w + GAP), BODY_W - 2 * (w + GAP), TX_X_AI));
    for (int i = 0; i < CU_N_AI; i++) {
        snprintf(ch, sizeof ch, "AI%d", i + 1);
        tx_t name = i < TXL_IO_AI_LEN ? TXL_IO_AI[i] : TX_NONE;
        U.x_ai[i] = io_row(l, ch, name == TX_NONE ? tr(TX_X_RESERVE) : tr(name), name == TX_NONE);
    }
}

static void pin_cb(lv_event_t *e)
{
    U.x_pin = (uint8_t)(intptr_t)lv_event_get_user_data(e);
}

static void build_x_pins(lv_obj_t *body)
{
    int32_t bw = (BODY_W - 7 * 8) / 8;
    for (int i = 0; i < 32; i++) {
        lv_obj_t *b = box(body, (i % 8) * (bw + 8), (i / 8) * 52, bw, 46, C_SURFACE, 10);
        border(b, C_LINE, 1);
        char n[4];
        snprintf(n, sizeof n, "%02d", i + 1);
        lv_obj_t *l = text(b, &font_18, C_INK, n);
        lv_obj_center(l);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(b, pin_cb, LV_EVENT_CLICKED, (void *)(intptr_t)(i + 1));
        U.x_pin_btn[i] = b;
    }
    lv_obj_t *info = box(body, 0, 4 * 52 + 4, BODY_W, XBODY_H - 4 * 52 - 4, C_SURFACE, 16);
    border(info, C_LINE, 1);
    lv_obj_set_style_pad_all(info, 16, 0);
    lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(info, 6, 0);
    U.x_pin_name = text(info, &font_22_semi, C_INK, "");
    U.x_pin_desc = text(info, &font_18, C_MUTED, "");
    U.x_pin = 1;
}

static void lim_cb(lv_event_t *e)
{
    int d = (int)(intptr_t)lv_event_get_user_data(e);
    const cu_state_t *s = &U.st;
    if (!s->victron.limit_enabled) {
        return;
    }
    if (d == 0) {
        if (confirm_tap(&U.lim_confirm, 1)) {
            cu_cmd_t c = {.type = CMD_VICTRON_LIMIT, .value = U.x_lim_value};
            send(&c);
        }
        return;
    }
    if (isnan(U.x_lim_value)) {
        U.x_lim_value = isnan(s->energy.v[EN_CURRENT_LIMIT]) ? s->victron.limit_min_a : s->energy.v[EN_CURRENT_LIMIT];
    }
    U.x_lim_value += (float)d;
    if (U.x_lim_value < s->victron.limit_min_a) {
        U.x_lim_value = s->victron.limit_min_a;
    }
    if (U.x_lim_value > s->victron.limit_max_a) {
        U.x_lim_value = s->victron.limit_max_a;
    }
    U.lim_confirm.armed = -1;
}

static void build_x_ac(lv_obj_t *body)
{
    int32_t w = (BODY_W - 3 * GAP) / 4;
    lv_obj_t *p = xpanel(body, 0, w, TX_X_AC_IN);
    static const tx_t in_rows[4] = {TX_X_AC_VOLTAGE, TX_X_AC_CURRENT, TX_X_AC_FREQ, TX_X_AC_POWER};
    for (int i = 0; i < 4; i++) {
        U.x_ac_in[i] = row_f(p, in_rows[i], &font_15_semi);
    }
    p = xpanel(body, w + GAP, w, TX_X_AC_OUT);
    static const tx_t out_rows[5] = {TX_X_AC_VOLTAGE, TX_X_AC_CURRENT, TX_X_AC_FREQ, TX_X_AC_POWER, TX_X_AC_APPARENT};
    for (int i = 0; i < 5; i++) {
        U.x_ac_out[i] = row_f(p, out_rows[i], &font_15_semi);
    }
    p = xpanel(body, 2 * (w + GAP), w, TX_X_AC_DEV);
    static const tx_t dev_rows[7] = {TX_X_AC_MODEL, TX_X_AC_STATE, TX_X_AC_MODE, TX_X_AC_OVERLOAD,
                                     TX_X_AC_LOW_BAT, TX_X_AC_TEMP, TX_X_AC_RIPPLE};
    for (int i = 0; i < 7; i++) {
        U.x_ac_dev[i] = row_f(p, dev_rows[i], &font_15_semi);
    }
    p = xpanel(body, 3 * (w + GAP), BODY_W - 3 * (w + GAP), TX_X_LIM);
    lv_obj_t *lim = box(p, 0, 0, LV_PCT(100), 64, C_SURFACE, 0);
    lv_obj_t *m = step_cb_btn(lim, "\xe2\x88\x92", lim_cb, -1, 64);
    lv_obj_align(m, LV_ALIGN_LEFT_MID, 0, 0);
    U.x_lim = text(lim, &font_34_semi, C_INK, "\xe2\x80\x93");
    lv_obj_align(U.x_lim, LV_ALIGN_CENTER, 0, 0);
    lv_obj_t *pl = step_cb_btn(lim, "+", lim_cb, 1, 64);
    lv_obj_align(pl, LV_ALIGN_RIGHT_MID, 0, 0);
    U.x_lim_set = big_btn(p, 56, lim_cb, (void *)(intptr_t)0, &U.x_lim_set_label);
    U.x_ctl_note = note(p, "");
    U.x_lim_value = NAN;
    U.lim_confirm.armed = -1;
}

static void build_x_conn(lv_obj_t *body)
{
    int32_t w = (BODY_W - GAP) / 2;
    lv_obj_t *p = xpanel(body, 0, w, TX_X_CONN_CERBO);
    static const tx_t cr[3] = {TX_X_CONN_STATE, TX_X_CONN_PORTAL, TX_X_LIM};
    for (int i = 0; i < 3; i++) {
        U.x_conn[i] = row_f(p, cr[i], &font_16_semi);
    }
    filler(p);
    note(p, tr(TX_X_CONN_NOTE));
    p = xpanel(body, w + GAP, BODY_W - w - GAP, TX_X_CONN_UNIT);
    static const tx_t ur[4] = {TX_X_CONN_VERSION, TX_X_CONN_HAL, TX_X_CONN_PROFILE, TX_X_CONN_WS};
    for (int i = 0; i < 4; i++) {
        U.x_unit[i] = row_f(p, ur[i], &font_16_semi);
    }
}

/* ----- network (stored in the simulator; applied on the device with M4) ----- */

static bool ipv4(const char *s, bool prefix)
{
    unsigned a, b, c, d, p = 32;
    char tail;
    int n = prefix ? sscanf(s, "%u.%u.%u.%u/%u%c", &a, &b, &c, &d, &p, &tail)
                   : sscanf(s, "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail);
    if (prefix ? n != 5 : n != 4) {
        return false;
    }
    return a < 256 && b < 256 && c < 256 && d < 256 && p >= 8 && p <= 30;
}

static bool net_valid(net_field_t f, const char *v)
{
    size_t n = strlen(v);
    switch (f) {
    case NF_ADDR: return ipv4(v, true);
    case NF_GW:
    case NF_HOST: return ipv4(v, false);
    case NF_DNS: return n == 0 || ipv4(v, false);
    case NF_SSID: return n >= 1 && n <= 32;
    case NF_PSK: return n >= 8 && n <= 63;
    }
    return false;
}

static void kb_close(void)
{
    if (U.kb_layer) {
        lv_obj_delete(U.kb_layer);
        U.kb_layer = NULL;
    }
}

static void kb_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        const char *v = lv_textarea_get_text(U.kb_ta);
        if (!net_valid(U.kb_field, v)) {
            lv_textarea_set_placeholder_text(U.kb_ta, tr(TX_X_NET_INVALID));
            lv_textarea_set_text(U.kb_ta, "");
            return;
        }
        snprintf(U.net[U.kb_field], sizeof U.net[0], "%s", v);
        U.net_confirm.armed = -1;
        kb_close();
    } else if (code == LV_EVENT_CANCEL) {
        kb_close();
    }
}

static void field_cb(lv_event_t *e)
{
    net_field_t f = (net_field_t)(intptr_t)lv_event_get_user_data(e);
    if (f <= NF_DNS && !U.net_static) {
        return;
    }
    U.kb_field = f;
    U.kb_layer = box(U.root, 0, 0, W, H, C_BG, 0);
    lv_obj_add_flag(U.kb_layer, LV_OBJ_FLAG_CLICKABLE);
    U.kb_ta = lv_textarea_create(U.kb_layer);
    lv_textarea_set_one_line(U.kb_ta, true);
    lv_obj_set_size(U.kb_ta, W - 2 * PAD, 60);
    lv_obj_set_pos(U.kb_ta, PAD, PAD);
    lv_obj_set_style_text_font(U.kb_ta, &font_22_semi, 0);
    lv_textarea_set_text(U.kb_ta, f == NF_PSK ? "" : U.net[f]);
    lv_textarea_set_password_mode(U.kb_ta, f == NF_PSK);
    U.kb = lv_keyboard_create(U.kb_layer);
    lv_obj_set_size(U.kb, W, H - 92);
    lv_obj_align(U.kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(U.kb, U.kb_ta);
    lv_keyboard_set_mode(U.kb, f == NF_SSID || f == NF_PSK ? LV_KEYBOARD_MODE_TEXT_LOWER : LV_KEYBOARD_MODE_NUMBER);
    lv_obj_add_event_cb(U.kb, kb_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(U.kb, kb_cb, LV_EVENT_CANCEL, NULL);
}

static void eth_cb(lv_event_t *e)
{
    U.net_static = (int)(intptr_t)lv_event_get_user_data(e) == 1;
    seg_set(U.x_eth_mode, 2, U.net_static);
    U.net_confirm.armed = -1;
}

static void apply_cb(lv_event_t *e)
{
    (void)e;
    bool ok = net_valid(NF_SSID, U.net[NF_SSID]) && net_valid(NF_PSK, U.net[NF_PSK]) && net_valid(NF_HOST, U.net[NF_HOST]);
    if (U.net_static) {
        ok = ok && net_valid(NF_ADDR, U.net[NF_ADDR]) && net_valid(NF_GW, U.net[NF_GW]) && net_valid(NF_DNS, U.net[NF_DNS]);
    }
    if (!ok) {
        set_text(U.x_net_note, tr(TX_X_NET_INVALID));
        return;
    }
    if (confirm_tap(&U.net_confirm, 1)) {
        set_text(U.x_net_note, tr(TX_X_NET_APPLIED)); /* TODO(M4): apply on the device (Ethernet, WLAN of the C6) */
    }
}

static lv_obj_t *field_row(lv_obj_t *p, tx_t name, net_field_t f)
{
    lv_obj_t *v = row_f(p, name, &font_16_semi);
    lv_obj_t *r = lv_obj_get_parent(v);
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(r, field_cb, LV_EVENT_CLICKED, (void *)(intptr_t)f);
    lv_obj_set_style_pad_ver(r, 10, 0);
    return v;
}

static void build_x_net(lv_obj_t *body)
{
    int32_t w = (BODY_W - GAP) / 2;
    lv_obj_t *p = xpanel(body, 0, w, TX_X_NET_ETH);
    static const tx_t em[2] = {TX_X_NET_DHCP, TX_X_NET_STATIC};
    seg(p, 2, em, eth_cb, U.x_eth_mode, NULL, 40);
    seg_set(U.x_eth_mode, 2, U.net_static);
    U.x_net_val[NF_ADDR] = field_row(p, TX_X_NET_ADDRESS, NF_ADDR);
    U.x_net_val[NF_GW] = field_row(p, TX_X_NET_GATEWAY, NF_GW);
    U.x_net_val[NF_DNS] = field_row(p, TX_X_NET_DNS, NF_DNS);
    filler(p);
    note(p, tr(TX_X_NET_NOTE_V2));
    p = xpanel(body, w + GAP, BODY_W - w - GAP, TX_X_NET_WIFI);
    U.x_net_val[NF_SSID] = field_row(p, TX_X_NET_SSID, NF_SSID);
    U.x_net_val[NF_PSK] = field_row(p, TX_X_NET_PSK, NF_PSK);
    U.x_net_val[NF_HOST] = field_row(p, TX_X_NET_CERBO_HOST, NF_HOST);
    filler(p);
    U.x_net_apply = big_btn(p, 52, apply_cb, NULL, &U.x_net_apply_label);
    U.x_net_note = note(p, tr(TX_X_NET_HINT));
    U.net_confirm.armed = -1;
}

static void build_expert(lv_obj_t *page)
{
    static const tx_t tabs[X_COUNT] = {TX_X_TAB_IO, TX_X_TAB_PINS, TX_X_TAB_AC, TX_X_TAB_CONN, TX_X_TAB_NET, TX_X_TAB_MEASURE};
    seg(page, X_COUNT, tabs, xtab_cb, U.x_tab_btn, NULL, 52);
    for (int i = 0; i < X_COUNT; i++) {
        U.x_body[i] = box(page, 0, XBODY_Y, BODY_W, XBODY_H, C_BG, 0);
        transparent(U.x_body[i]);
    }
    build_x_io(U.x_body[X_IO]);
    build_x_pins(U.x_body[X_PINS]);
    build_x_ac(U.x_body[X_AC]);
    build_x_conn(U.x_body[X_CONN]);
    build_x_net(U.x_body[X_NET]);
    lv_obj_t *m = xpanel(U.x_body[X_MEASURE], 0, BODY_W, TX_X_MEASURE_TITLE);
    note(m, tr(TX_X_MEASURE_NOTE));
    ui_expert_tab(U.xtab);
}

static const char *alarm_text(float v)
{
    static const tx_t al[3] = {TX_X_AC_ALARM_0, TX_X_AC_ALARM_1, TX_X_AC_ALARM_2};
    return isnan(v) || v < 0 || v > 2 ? "\xe2\x80\x93" : tr(al[(int)v]);
}

static void update_expert(const cu_state_t *s)
{
    char a[96];
    if (U.xtab == X_IO) {
        for (int i = 0; i < CU_N_DI; i++) {
            bool on = (s->io.di >> i) & 1u;
            set_text(U.x_di[i], on ? "1" : "0");
            lv_obj_set_style_text_color(U.x_di[i], lv_color_hex(on ? C_OK : C_MUTED), 0);
        }
        for (int i = 0; i < CU_N_RELAYS + CU_N_LADDER; i++) {
            bool on = i < CU_N_RELAYS ? (s->io.relays >> i) & 1u : (s->io.ladder >> (i - CU_N_RELAYS)) & 1u;
            set_text(U.x_k[i], tr(on ? TX_COMMON_ON : TX_COMMON_OFF));
            lv_obj_set_style_text_color(U.x_k[i], lv_color_hex(on ? C_OK : C_MUTED), 0);
        }
        for (int i = 0; i < CU_N_AI; i++) {
            set_text(U.x_ai[i], num(a, sizeof a, s->io.ai[i], 3, "V"));
        }
    } else if (U.xtab == X_PINS) {
        for (int i = 0; i < 32; i++) {
            bool on = i + 1 == U.x_pin;
            lv_obj_set_style_bg_color(U.x_pin_btn[i], lv_color_hex(on ? C_ACCENT : C_SURFACE), 0);
            lv_obj_set_style_text_color(lv_obj_get_child(U.x_pin_btn[i], 0), lv_color_hex(on ? C_ACCENT_INK : C_INK), 0);
        }
        snprintf(a, sizeof a, "T32b/%02u", U.x_pin);
        set_text(U.x_pin_name, a);
        tx_t d = U.x_pin >= 1 && U.x_pin <= TXL_PINS_LEN ? TXL_PINS[U.x_pin - 1] : TX_NONE;
        set_text(U.x_pin_desc, d == TX_NONE ? tr(TX_X_RESERVE) : tr(d));
    } else if (U.xtab == X_AC) {
        float active = ev(s, EN_ACTIVE_INPUT);
        bool shore = !isnan(active) && active != 240;
        set_text(U.x_ac_in[0], num(a, sizeof a, shore ? ev(s, EN_IN_V) : NAN, 0, "V"));
        set_text(U.x_ac_in[1], num(a, sizeof a, shore ? ev(s, EN_IN_I) : NAN, 1, "A"));
        set_text(U.x_ac_in[2], num(a, sizeof a, shore ? ev(s, EN_IN_F) : NAN, 1, "Hz"));
        set_text(U.x_ac_in[3], num(a, sizeof a, shore ? ev(s, EN_IN_P) : NAN, 0, "W"));
        set_text(U.x_ac_out[0], num(a, sizeof a, ev(s, EN_OUT_V), 0, "V"));
        set_text(U.x_ac_out[1], num(a, sizeof a, ev(s, EN_OUT_I), 1, "A"));
        set_text(U.x_ac_out[2], num(a, sizeof a, ev(s, EN_OUT_F), 1, "Hz"));
        set_text(U.x_ac_out[3], num(a, sizeof a, ev(s, EN_OUT_P), 0, "W"));
        set_text(U.x_ac_out[4], num(a, sizeof a, ev(s, EN_OUT_S), 0, "VA"));
        set_text(U.x_ac_dev[0], tr(TX_X_AC_MODEL_VAL));
        float st = ev(s, EN_VEBUS_STATE), md = ev(s, EN_VEBUS_MODE);
        static const tx_t vb[12] = {TX_VEBUS_0, TX_VEBUS_1, TX_VEBUS_2, TX_VEBUS_3, TX_VEBUS_4,  TX_VEBUS_5,
                                    TX_VEBUS_6, TX_VEBUS_7, TX_VEBUS_8, TX_VEBUS_9, TX_VEBUS_10, TX_VEBUS_11};
        static const tx_t mdn[4] = {TX_X_AC_MODE_1, TX_X_AC_MODE_2, TX_X_AC_MODE_3, TX_X_AC_MODE_4};
        set_text(U.x_ac_dev[1], !isnan(st) && st >= 0 && st <= 11 ? tr(vb[(int)st]) : "\xe2\x80\x93");
        set_text(U.x_ac_dev[2], !isnan(md) && md >= 1 && md <= 4 ? tr(mdn[(int)md - 1]) : "\xe2\x80\x93");
        set_text(U.x_ac_dev[3], alarm_text(ev(s, EN_ALARM_OVERLOAD)));
        set_text(U.x_ac_dev[4], alarm_text(ev(s, EN_ALARM_LOW_BATTERY)));
        set_text(U.x_ac_dev[5], alarm_text(ev(s, EN_ALARM_TEMPERATURE)));
        set_text(U.x_ac_dev[6], alarm_text(ev(s, EN_ALARM_RIPPLE)));
        float shown = isnan(U.x_lim_value) ? ev(s, EN_CURRENT_LIMIT) : U.x_lim_value;
        set_text(U.x_lim, num(a, sizeof a, shown, 1, "A"));
        set_text(U.x_lim_set_label, armed(&U.lim_confirm, 1) ? tr(TX_X_CONFIRM) : tr(TX_X_LIM_SET));
        enable(U.x_lim_set, s->victron.limit_enabled && s->energy.connected);
        tx_t n = !s->victron.enabled         ? TX_X_CTL_DISABLED
                 : !s->victron.limit_enabled ? TX_X_CTL_LIMIT_DISABLED
                 : !s->energy.connected      ? TX_X_CTL_OFFLINE
                 : s->victron.pending_limit  ? TX_X_CTL_PENDING
                 : s->victron.unconfirmed == VW_CURRENT_LIMIT ? TX_X_CTL_UNCONFIRMED
                 : s->victron.last == VC_LIMIT_FIXED          ? TX_X_VC_LIMIT_FIXED
                 : s->victron.last == VC_OUT_OF_RANGE         ? TX_X_VC_OUT_OF_RANGE
                 : s->victron.last == VC_TOO_FAST             ? TX_X_VC_TOO_FAST
                                                              : TX_X_CTL_HINT;
        set_text(U.x_ctl_note, tr(n));
    } else if (U.xtab == X_CONN) {
        set_text(U.x_conn[0], tr(s->energy.connected ? TX_X_CONN_ONLINE : TX_X_CONN_OFFLINE));
        set_text(U.x_conn[1], "\xe2\x80\x93"); /* TODO(M4): portal id from the cerbo component */
        set_text(U.x_conn[2], num(a, sizeof a, ev(s, EN_CURRENT_LIMIT), 1, "A"));
        set_text(U.x_unit[0], "2.0.0-m2");
        set_text(U.x_unit[1], tr(U.cfg.simulation ? TX_X_CONN_HAL_SIM : TX_COMMON_UNKNOWN));
        set_text(U.x_unit[2], tr(U.cfg.simulation ? TX_X_CONN_PROFILE_SIM : TX_X_CONN_PROFILE_REAL));
        set_text(U.x_unit[3], tr(TX_X_CONN_WS_VAL));
    } else if (U.xtab == X_NET) {
        for (int f = 0; f < NET_FIELDS; f++) {
            const char *v = U.net[f];
            if (f == NF_PSK) {
                v = tr(U.net[NF_PSK][0] ? TX_X_NET_PSK_SET : TX_X_NET_PSK_UNSET);
            } else if (f <= NF_DNS && !U.net_static) {
                v = tr(TX_X_NET_DHCP);
            } else if (!v[0]) {
                v = "\xe2\x80\x93";
            }
            set_text(U.x_net_val[f], v);
        }
        set_text(U.x_net_apply_label, armed(&U.net_confirm, 1) ? tr(TX_X_CONFIRM) : tr(TX_X_NET_APPLY));
    }
}

static void update(lv_timer_t *t)
{
    (void)t;
    if (!U.built) {
        return;
    }
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
    update_energy(s);
    update_heat(s);
    update_water(s);
    if (U.page == UI_PAGE_EXPERT) {
        update_expert(s);
    }
    /* Back to the overview after inactivity, never while the roof moves (SPEC 9). */
    if (U.page != UI_PAGE_OVERVIEW && !s->roof.busy && U.roof_dir == ROOF_STOP && !U.kb_layer &&
        lv_display_get_inactive_time(NULL) > (uint32_t)U.cfg.return_min * 60000u) {
        ui_show(UI_PAGE_OVERVIEW);
    }
    apply_theme(); /* automatic: night with the dashboard lighting (58d) */
}

static lv_obj_t *g_parent;

static void build_all(void)
{
    U.root = box(g_parent, 0, 0, W, H, C_BG, 0);
    lv_obj_set_style_text_color(U.root, lv_color_hex(C_INK), 0);
    build_header();
    U.body = box(U.root, PAD, HEAD + PAD, BODY_W, BODY_H, C_BG, 0);
    for (int i = 0; i < UI_PAGE_COUNT; i++) {
        U.pages[i] = box(U.body, 0, 0, BODY_W, BODY_H, C_BG, 0);
    }
    build_overview(U.pages[UI_PAGE_OVERVIEW]);
    build_roof(U.pages[UI_PAGE_ROOF]);
    build_fridge(U.pages[UI_PAGE_FRIDGE]);
    build_energy(U.pages[UI_PAGE_ENERGY]);
    build_heat(U.pages[UI_PAGE_HEAT]);
    build_water(U.pages[UI_PAGE_WATER]);
    build_settings(U.pages[UI_PAGE_SETTINGS]);
    build_expert(U.pages[UI_PAGE_EXPERT]);
    U.built = true;
}

/* Theme change: rebuild every page with the other palette, keep page and settings. */
static void rebuild(void)
{
    ui_page_t page = U.page;
    U.built = false;
    kb_close();
    lv_obj_delete(U.root);
    build_all();
    U.page = UI_PAGE_OVERVIEW;
    ui_show(page);
    update(NULL);
}

void ui_init(lv_obj_t *parent, const ui_config_t *cfg)
{
    memset(&U, 0, sizeof U);
    U.cfg = *cfg;
    U.brightness = 80;
    U.theme = THEME_AUTO;
    U.night = true;
    PAL = &NIGHT;
    snprintf(U.net[NF_HOST], sizeof U.net[0], "172.24.24.1");
    g_parent = parent;
    build_all();
    ui_show(UI_PAGE_OVERVIEW);
    if (!U.timers) {
        lv_timer_create(roof_timer, 100, NULL);
        lv_timer_create(update, 100, NULL);
        U.timers = true;
    }
    update(NULL);
}
