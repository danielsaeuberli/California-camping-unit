#include "profile.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"

#define TODO "TODO_MEASURE"

typedef struct {
    char *err;
    size_t len;
    bool failed;
} ctx_t;

static void fail(ctx_t *c, const char *fmt, ...)
{
    if (c->failed) {
        return;
    }
    c->failed = true;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(c->err, c->len, fmt, ap);
    va_end(ap);
}

/* Every key of obj must be in allowed (NULL-terminated). */
static void check_keys(ctx_t *c, const cJSON *obj, const char *path, const char *const *allowed)
{
    const cJSON *it;
    cJSON_ArrayForEach(it, obj)
    {
        bool ok = false;
        for (const char *const *a = allowed; *a; a++) {
            if (strcmp(it->string, *a) == 0) {
                ok = true;
                break;
            }
        }
        if (!ok) {
            fail(c, "unknown key %s.%s", path, it->string);
        }
    }
}

static const cJSON *obj(ctx_t *c, const cJSON *parent, const char *path, const char *key, const char *const *allowed)
{
    const cJSON *o = cJSON_GetObjectItemCaseSensitive(parent, key);
    if (!cJSON_IsObject(o)) {
        fail(c, "missing object %s.%s", path, key);
        return NULL;
    }
    char p[64];
    snprintf(p, sizeof p, "%s.%s", path, key);
    check_keys(c, o, p, allowed);
    return o;
}

static const cJSON *item(ctx_t *c, const cJSON *o, const char *key)
{
    const cJSON *v = o ? cJSON_GetObjectItemCaseSensitive(o, key) : NULL;
    if (!v) {
        fail(c, "missing key %s", key);
    }
    return v;
}

static bool is_todo(const cJSON *v)
{
    return cJSON_IsString(v) && strcmp(v->valuestring, TODO) == 0;
}

static double num(ctx_t *c, const cJSON *o, const char *key)
{
    const cJSON *v = item(c, o, key);
    if (v && !cJSON_IsNumber(v)) {
        fail(c, "%s must be a number", key);
    }
    return cJSON_IsNumber(v) ? v->valuedouble : 0;
}

static bool boolean(ctx_t *c, const cJSON *o, const char *key)
{
    const cJSON *v = item(c, o, key);
    if (v && !cJSON_IsBool(v)) {
        fail(c, "%s must be true or false", key);
    }
    return cJSON_IsTrue(v);
}

static cu_cal_t cal(ctx_t *c, const cJSON *o, const char *key)
{
    const cJSON *v = item(c, o, key);
    if (is_todo(v)) {
        return (cu_cal_t){0, false};
    }
    if (!cJSON_IsNumber(v) || v->valuedouble <= 0) {
        fail(c, "%s must be a positive number or \"" TODO "\"", key);
        return (cu_cal_t){0, false};
    }
    return (cu_cal_t){(float)v->valuedouble, true};
}

static void str(ctx_t *c, const cJSON *o, const char *key, char *dst, size_t len)
{
    const cJSON *v = item(c, o, key);
    if (!cJSON_IsString(v) || strlen(v->valuestring) >= len) {
        fail(c, "%s must be a string shorter than %u", key, (unsigned)len);
        return;
    }
    strcpy(dst, v->valuestring);
}

/* "K1" / "DI10" / "AI8" -> channel number, checked against max. */
static uint8_t channel(ctx_t *c, const cJSON *o, const char *name, const char *prefix, int max)
{
    const cJSON *ch = item(c, o, "channel");
    if (!cJSON_IsString(ch)) {
        fail(c, "%s.channel must be a string", name);
        return 0;
    }
    size_t pl = strlen(prefix);
    int n = 0;
    if (strncmp(ch->valuestring, prefix, pl) != 0 || sscanf(ch->valuestring + pl, "%d", &n) != 1 || n < 1 || n > max) {
        fail(c, "%s.channel %s is not %s1..%s%d", name, ch->valuestring, prefix, prefix, max);
        return 0;
    }
    return (uint8_t)n;
}

static void ntc(ctx_t *c, const cJSON *curves, const char *key, profile_ntc_t *out)
{
    const cJSON *v = item(c, curves, key);
    out->cal = false;
    if (is_todo(v)) {
        return;
    }
    if (!cJSON_IsArray(v) || cJSON_GetArraySize(v) != 3) {
        fail(c, "curves.%s must be three [temp_c, ohm] points or \"" TODO "\"", key);
        return;
    }
    for (int i = 0; i < 3; i++) {
        const cJSON *p = cJSON_GetArrayItem(v, i);
        const cJSON *t = cJSON_GetArrayItem(p, 0), *r = cJSON_GetArrayItem(p, 1);
        if (!cJSON_IsArray(p) || cJSON_GetArraySize(p) != 2 || !cJSON_IsNumber(t) || !cJSON_IsNumber(r) || r->valuedouble <= 0) {
            fail(c, "curves.%s point %d must be [temp_c, ohm > 0]", key, i);
            return;
        }
        out->temp_c[i] = (float)t->valuedouble;
        out->ohm[i] = (float)r->valuedouble;
    }
    out->cal = true;
}

static bool pair(const cJSON *v, uint8_t out[2])
{
    if (!cJSON_IsArray(v) || cJSON_GetArraySize(v) != 2) {
        return false;
    }
    for (int i = 0; i < 2; i++) {
        const cJSON *b = cJSON_GetArrayItem(v, i);
        if (!cJSON_IsNumber(b) || (b->valueint != 0 && b->valueint != 1)) {
            return false;
        }
        out[i] = (uint8_t)b->valueint;
    }
    return true;
}

static roof_meaning_t meaning(ctx_t *c, const cJSON *o, const char *name)
{
    const cJSON *v = item(c, o, "meaning");
    if (is_todo(v)) {
        return ROOF_MEANING_UNKNOWN;
    }
    if (cJSON_IsString(v) && strcmp(v->valuestring, "open") == 0) {
        return ROOF_MEANING_OPEN;
    }
    if (cJSON_IsString(v) && strcmp(v->valuestring, "close") == 0) {
        return ROOF_MEANING_CLOSE;
    }
    fail(c, "outputs.%s.meaning must be open, close or \"" TODO "\"", name);
    return ROOF_MEANING_UNKNOWN;
}

/* Number or "TODO_MEASURE" (any sign). */
static cu_cal_t cal_any(ctx_t *c, const cJSON *o, const char *key)
{
    const cJSON *v = item(c, o, key);
    if (is_todo(v)) {
        return (cu_cal_t){0, false};
    }
    if (!cJSON_IsNumber(v)) {
        fail(c, "%s must be a number or \"" TODO "\"", key);
        return (cu_cal_t){0, false};
    }
    return (cu_cal_t){(float)v->valuedouble, true};
}

static drive_t drive(ctx_t *c, const cJSON *o, const char *name)
{
    static const char *const names[] = {"relay_plus", "relay_ground", "open_drain", "dry_contact"};
    const cJSON *v = item(c, o, "drive");
    if (is_todo(v)) {
        return DRIVE_UNKNOWN;
    }
    for (int i = 0; i < 4; i++) {
        if (cJSON_IsString(v) && strcmp(v->valuestring, names[i]) == 0) {
            return (drive_t)(i + 1);
        }
    }
    fail(c, "outputs.%s.drive must be relay_plus, relay_ground, open_drain, dry_contact or \"" TODO "\"", name);
    return DRIVE_UNKNOWN;
}

static uint32_t ms(ctx_t *c, const cJSON *o, const char *key, double factor)
{
    double v = num(c, o, key);
    if (v < 0) {
        fail(c, "%s must not be negative", key);
    }
    return (uint32_t)(v * factor + 0.5);
}

static void parse_tree(ctx_t *c, const cJSON *root, profile_t *p)
{
    static const char *const top[] = {"schema", "vehicle", "outputs", "inputs", "analog", "curves", "roof", "speed",
                                      "fridge", "heater", "power", "cerbo", "victron", "measure", "ui", "release", NULL};
    static const char *const ch_pin[] = {"channel", "pin", NULL};
    static const char *const ch_pin_drive[] = {"channel", "pin", "drive", NULL};
    static const char *const ch_pin_meaning[] = {"channel", "pin", "meaning", "drive", NULL};
    static const char *const ch_pin_pullup[] = {"channel", "pin", "pullup_ohm", NULL};
    static const char *const ch_pin_div[] = {"channel", "pin", "divider", NULL};

    if (!cJSON_IsObject(root)) {
        fail(c, "profile is not a JSON object");
        return;
    }
    check_keys(c, root, "", top);
    if (num(c, root, "schema") != 1) {
        fail(c, "schema must be 1");
    }

    static const char *const outs[] = {"roof_a", "roof_b", "fridge", "heater", NULL};
    const cJSON *o = obj(c, root, "", "outputs", outs);
    const cJSON *e;
    e = obj(c, o, "outputs", "roof_a", ch_pin_meaning);
    p->k_roof_a = channel(c, e, "roof_a", "K", CU_N_RELAYS);
    p->roof_a_meaning = meaning(c, e, "roof_a");
    p->drive_roof_a = drive(c, e, "roof_a");
    e = obj(c, o, "outputs", "roof_b", ch_pin_meaning);
    p->k_roof_b = channel(c, e, "roof_b", "K", CU_N_RELAYS);
    p->roof_b_meaning = meaning(c, e, "roof_b");
    p->drive_roof_b = drive(c, e, "roof_b");
    e = obj(c, o, "outputs", "fridge", ch_pin_drive);
    p->k_fridge = channel(c, e, "fridge", "K", CU_N_RELAYS);
    p->drive_fridge = drive(c, e, "fridge");
    e = obj(c, o, "outputs", "heater", ch_pin_drive);
    p->k_heater = channel(c, e, "heater", "K", CU_N_RELAYS);
    p->drive_heater = drive(c, e, "heater");

    static const char *const ins[] = {"roof_fb_a", "roof_fb_b", "fridge_diag", "heater_run", "charger_a11",
                                      "speed", "dim58", "key86s", "dplus", "ignition", NULL};
    o = obj(c, root, "", "inputs", ins);
    uint8_t *di[] = {&p->di_roof_a, &p->di_roof_b, &p->di_fridge_diag, &p->di_heater_run, &p->di_charger_a11,
                     &p->di_speed, &p->di_dim58, &p->di_key86s, &p->di_dplus, &p->di_ignition};
    for (size_t i = 0; i < CU_ARRAY_LEN(di); i++) {
        *di[i] = channel(c, obj(c, o, "inputs", ins[i], ch_pin), ins[i], "DI", CU_N_DI);
    }

    static const char *const ans[] = {"fridge_temp", "fresh", "waste", "outside", "ref_3v3", "battery", NULL};
    o = obj(c, root, "", "analog", ans);
    struct {
        uint8_t *ch;
        cu_cal_t *val;
        const char *const *keys;
        const char *field;
    } ai[] = {
        {&p->ai_fridge, &p->pullup_fridge, ch_pin_pullup, "pullup_ohm"},
        {&p->ai_fresh, &p->pullup_fresh, ch_pin_pullup, "pullup_ohm"},
        {&p->ai_waste, &p->pullup_waste, ch_pin_pullup, "pullup_ohm"},
        {&p->ai_outside, &p->pullup_outside, ch_pin_pullup, "pullup_ohm"},
        {&p->ai_ref, &p->div_ref, ch_pin_div, "divider"},
        {&p->ai_battery, &p->div_battery, ch_pin_div, "divider"},
    };
    for (size_t i = 0; i < CU_ARRAY_LEN(ai); i++) {
        e = obj(c, o, "analog", ans[i], ai[i].keys);
        *ai[i].ch = channel(c, e, ans[i], "AI", CU_N_AI);
        *ai[i].val = cal(c, e, ai[i].field);
    }

    static const char *const curve_keys[] = {"g817", "g249", "g120_steps_ohm", "g126_full_below_ohm", NULL};
    o = obj(c, root, "", "curves", curve_keys);
    ntc(c, o, "g817", &p->g817);
    ntc(c, o, "g249", &p->g249);
    const cJSON *steps = item(c, o, "g120_steps_ohm");
    p->g120_cal = false;
    if (cJSON_IsArray(steps) && cJSON_GetArraySize(steps) == 3) {
        p->g120_cal = true;
        for (int i = 0; i < 3; i++) {
            const cJSON *s = cJSON_GetArrayItem(steps, i);
            if (!cJSON_IsNumber(s) || s->valuedouble <= 0) {
                fail(c, "curves.g120_steps_ohm must hold three positive numbers");
            }
            p->g120_steps_ohm[i] = cJSON_IsNumber(s) ? (float)s->valuedouble : 0;
        }
    } else if (steps && !is_todo(steps)) {
        fail(c, "curves.g120_steps_ohm must hold three numbers or \"" TODO "\"");
    }
    p->g126_full_below_ohm = cal(c, o, "g126_full_below_ohm");

    static const char *const roof_keys[] = {"hold_timeout_ms", "max_run_s", "reverse_gap_ms", "require_ignition",
                                            "tap_to_run", "pump_afterrun_s", "truth_table", NULL};
    o = obj(c, root, "", "roof", roof_keys);
    p->roof_hold_timeout_ms = ms(c, o, "hold_timeout_ms", 1);
    p->roof_max_run_ms = ms(c, o, "max_run_s", 1000);
    p->roof_reverse_gap_ms = ms(c, o, "reverse_gap_ms", 1);
    p->roof_require_ignition = boolean(c, o, "require_ignition");
    p->roof_tap_to_run = boolean(c, o, "tap_to_run");
    const cJSON *pa = item(c, o, "pump_afterrun_s");
    p->roof_pump_afterrun_ms = (cu_cal_t){0, false};
    if (cJSON_IsNumber(pa) && pa->valuedouble >= 0) {
        p->roof_pump_afterrun_ms = (cu_cal_t){(float)(pa->valuedouble * 1000.0), true};
    } else if (pa && !is_todo(pa)) {
        fail(c, "roof.pump_afterrun_s must be a number >= 0 or \"" TODO "\"");
    }
    const cJSON *tt = item(c, o, "truth_table");
    p->roof_table_cal = false;
    if (cJSON_IsObject(tt)) {
        static const char *const tt_keys[] = {"closed", "open", "between", NULL};
        check_keys(c, tt, "roof.truth_table", tt_keys);
        p->roof_table_cal = pair(cJSON_GetObjectItemCaseSensitive(tt, "closed"), p->roof_closed) &&
                            pair(cJSON_GetObjectItemCaseSensitive(tt, "open"), p->roof_open) &&
                            pair(cJSON_GetObjectItemCaseSensitive(tt, "between"), p->roof_between);
        if (!p->roof_table_cal) {
            fail(c, "roof.truth_table needs closed, open and between as [DI1, DI2] with 0/1");
        }
    } else if (tt && !is_todo(tt)) {
        fail(c, "roof.truth_table must be an object or \"" TODO "\"");
    }

    static const char *const speed_keys[] = {"window_ms", "moving_min_edges", NULL};
    o = obj(c, root, "", "speed", speed_keys);
    p->speed_window_ms = ms(c, o, "window_ms", 1);
    p->speed_moving_min_edges = cal(c, o, "moving_min_edges");

    static const char *const fridge_keys[] = {"target_default_c", "target_min_c", "target_max_c", "on_above_c",
                                              "off_below_c", "min_run_s", "min_pause_s", "diag_polarity",
                                              "diag_decoder", NULL};
    o = obj(c, root, "", "fridge", fridge_keys);
    p->fridge_target_default_c = cal_any(c, o, "target_default_c");
    p->fridge_target_min_c = cal_any(c, o, "target_min_c");
    p->fridge_target_max_c = cal_any(c, o, "target_max_c");
    p->fridge_on_above_c = cal(c, o, "on_above_c");
    p->fridge_off_below_c = cal(c, o, "off_below_c");
    p->fridge_min_run_ms = ms(c, o, "min_run_s", 1000);
    p->fridge_min_pause_ms = ms(c, o, "min_pause_s", 1000);
    const cJSON *pol = item(c, o, "diag_polarity");
    p->fridge_diag_polarity = DIAG_POL_UNKNOWN;
    if (cJSON_IsString(pol) && strcmp(pol->valuestring, "low") == 0) {
        p->fridge_diag_polarity = DIAG_POL_LOW;
    } else if (cJSON_IsString(pol) && strcmp(pol->valuestring, "high") == 0) {
        p->fridge_diag_polarity = DIAG_POL_HIGH;
    } else if (pol && !is_todo(pol)) {
        fail(c, "fridge.diag_polarity must be low, high or \"" TODO "\"");
    }
    const cJSON *dec = item(c, o, "diag_decoder");
    p->fridge_diag_decoder = DIAG_DECODER_NONE;
    if (cJSON_IsString(dec) && strcmp(dec->valuestring, "secop_hypothesis") == 0) {
        p->fridge_diag_decoder = DIAG_DECODER_SECOP_HYPOTHESIS;
    } else if (!cJSON_IsString(dec) || strcmp(dec->valuestring, "none") != 0) {
        fail(c, "fridge.diag_decoder must be none or secop_hypothesis");
    }
    if (p->fridge_diag_decoder != DIAG_DECODER_NONE && p->fridge_diag_polarity == DIAG_POL_UNKNOWN) {
        fail(c, "fridge.diag_decoder needs a measured diag_polarity");
    }

    static const char *const heater_keys[] = {"levels", "level_default", "start_timeout_s", "ladder_gap_ms",
                                              "level_ohm", "level_bits", "level_to_c", NULL};
    o = obj(c, root, "", "heater", heater_keys);
    double levels = num(c, o, "levels");
    if (levels < 1 || levels > 16) {
        fail(c, "heater.levels must be 1..16");
    }
    p->heater_levels = (uint8_t)levels;
    p->heater_level_default = (uint8_t)num(c, o, "level_default");
    if (p->heater_level_default >= p->heater_levels) {
        fail(c, "heater.level_default must be below heater.levels");
    }
    p->heater_start_timeout_ms = ms(c, o, "start_timeout_s", 1000);
    p->heater_ladder_gap_ms = ms(c, o, "ladder_gap_ms", 1);
    const cJSON *lo = item(c, o, "level_ohm");
    if (!cJSON_IsArray(lo) || cJSON_GetArraySize(lo) != p->heater_levels) {
        fail(c, "heater.level_ohm needs one entry per level");
    } else {
        for (int i = 0; i < p->heater_levels; i++) {
            const cJSON *r = cJSON_GetArrayItem(lo, i);
            if (is_todo(r)) {
                p->heater_level_ohm[i] = (cu_cal_t){0, false};
            } else if (cJSON_IsNumber(r) && r->valuedouble > 0) {
                p->heater_level_ohm[i] = (cu_cal_t){(float)r->valuedouble, true};
            } else {
                fail(c, "heater.level_ohm entries must be > 0 or \"" TODO "\"");
            }
        }
    }
    const cJSON *bits = item(c, o, "level_bits");
    p->heater_bits_cal = false;
    if (cJSON_IsArray(bits)) {
        if (cJSON_GetArraySize(bits) != p->heater_levels) {
            fail(c, "heater.level_bits needs one entry per level");
        } else {
            p->heater_bits_cal = true;
            for (int i = 0; i < p->heater_levels; i++) {
                const cJSON *b = cJSON_GetArrayItem(bits, i);
                if (!cJSON_IsNumber(b) || b->valueint < 0 || b->valueint > 15) {
                    fail(c, "heater.level_bits entries must be 0..15");
                }
                p->heater_level_bits[i] = cJSON_IsNumber(b) ? (uint8_t)b->valueint : 0;
            }
        }
    } else if (bits && !is_todo(bits)) {
        fail(c, "heater.level_bits must be a list or \"" TODO "\"");
    }
    item(c, o, "level_to_c"); /* display only, read by the UI later */

    static const char *const power_keys[] = {"idle_s", "undervoltage_v", "undervoltage_s", NULL};
    o = obj(c, root, "", "power", power_keys);
    p->power_idle_ms = ms(c, o, "idle_s", 1000);
    p->power_undervoltage_v = (float)num(c, o, "undervoltage_v");
    p->power_undervoltage_ms = ms(c, o, "undervoltage_s", 1000);

    static const char *const cerbo_keys[] = {"ssid", "password", "broker", "port", "keepalive_s", "stale_s", NULL};
    o = obj(c, root, "", "cerbo", cerbo_keys);
    str(c, o, "ssid", p->cerbo_ssid, sizeof p->cerbo_ssid);
    str(c, o, "password", p->cerbo_password, sizeof p->cerbo_password);
    str(c, o, "broker", p->cerbo_broker, sizeof p->cerbo_broker);
    p->cerbo_port = (uint16_t)num(c, o, "port");
    p->cerbo_keepalive_s = (uint32_t)num(c, o, "keepalive_s");
    p->cerbo_stale_s = (uint32_t)num(c, o, "stale_s");

    static const char *const vic_keys[] = {"control", "modes", "current_limit_a", "confirm_s", "min_interval_s", NULL};
    o = obj(c, root, "", "victron", vic_keys);
    p->victron_control = boolean(c, o, "control");
    const cJSON *modes = item(c, o, "modes");
    p->victron_modes = 0;
    if (!cJSON_IsArray(modes) || cJSON_GetArraySize(modes) < 1) {
        fail(c, "victron.modes must list the allowed modes 1..4");
    } else {
        const cJSON *m;
        cJSON_ArrayForEach(m, modes)
        {
            if (!cJSON_IsNumber(m) || m->valueint < 1 || m->valueint > 4) {
                fail(c, "victron.modes: allowed values are 1, 2, 3, 4");
            } else {
                p->victron_modes |= (uint8_t)(1u << m->valueint);
            }
        }
    }
    static const char *const lim_keys[] = {"min", "max", NULL};
    const cJSON *lim = obj(c, o, "victron", "current_limit_a", lim_keys);
    p->victron_limit_min_a = cal(c, lim, "min");
    p->victron_limit_max_a = cal(c, lim, "max");
    p->victron_confirm_ms = ms(c, o, "confirm_s", 1000);
    p->victron_min_interval_ms = ms(c, o, "min_interval_s", 1000);

    static const char *const measure_keys[] = {"outputs_locked", NULL};
    p->measure_outputs_locked = boolean(c, obj(c, root, "", "measure", measure_keys), "outputs_locked");

    static const char *const ui_keys[] = {"return_to_overview_min", NULL};
    double rm = num(c, obj(c, root, "", "ui", ui_keys), "return_to_overview_min");
    if (rm != 1 && rm != 2 && rm != 5) {
        fail(c, "ui.return_to_overview_min must be 1, 2 or 5");
    }
    p->ui_return_min = (uint8_t)rm;

    static const char *const rel_keys[] = {"roof", "fridge", "heater", NULL};
    o = obj(c, root, "", "release", rel_keys);
    p->release_roof = boolean(c, o, "roof");
    p->release_fridge = boolean(c, o, "fridge");
    p->release_heater = boolean(c, o, "heater");

    if (!c->failed && p->k_roof_a == p->k_roof_b) {
        fail(c, "roof_a and roof_b must use different relays");
    }
    if (!c->failed && p->roof_a_meaning != ROOF_MEANING_UNKNOWN && p->roof_a_meaning == p->roof_b_meaning) {
        fail(c, "roof_a and roof_b must have different meanings");
    }
}

static bool finish(cJSON *root, profile_t *out, char *err, size_t err_len)
{
    ctx_t c = {err, err_len, false};
    if (err_len) {
        err[0] = 0;
    }
    profile_t p;
    memset(&p, 0, sizeof p);
    if (!root) {
        fail(&c, "invalid JSON");
    } else {
        parse_tree(&c, root, &p);
    }
    cJSON_Delete(root);
    if (c.failed) {
        return false;
    }
    *out = p;
    return true;
}

bool profile_parse(const char *json, profile_t *out, char *err, size_t err_len)
{
    return finish(cJSON_Parse(json), out, err, err_len);
}

static void merge(cJSON *dst, const cJSON *src)
{
    const cJSON *it;
    cJSON_ArrayForEach(it, src)
    {
        if (it->string[0] == '_') {
            continue;
        }
        cJSON *cur = cJSON_GetObjectItemCaseSensitive(dst, it->string);
        if (cJSON_IsObject(cur) && cJSON_IsObject(it)) {
            merge(cur, it);
        } else {
            cJSON *copy = cJSON_Duplicate(it, true);
            if (cur) {
                cJSON_ReplaceItemInObjectCaseSensitive(dst, it->string, copy);
            } else {
                cJSON_AddItemToObject(dst, it->string, copy);
            }
        }
    }
}

bool profile_parse_with_overlay(const char *base, const char *overlay, profile_t *out, char *err, size_t err_len)
{
    cJSON *root = cJSON_Parse(base);
    if (root && overlay) {
        cJSON *ov = cJSON_Parse(overlay);
        if (!ov) {
            cJSON_Delete(root);
            snprintf(err, err_len, "invalid overlay JSON");
            return false;
        }
        merge(root, ov);
        cJSON_Delete(ov);
    }
    return finish(root, out, err, err_len);
}
