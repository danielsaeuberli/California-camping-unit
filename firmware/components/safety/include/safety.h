/* Safety rules that hold the output state (SPEC 6). */
#pragma once

#include "cu_common.h"
#include "hal_io.h"
#include "profile.h"

#define SAFETY_FAIL_LIMIT 3 /* S5, S11: three I2C errors in a row */

typedef struct {
    bool relay[CU_N_RELAYS]; /* index 0 = K1 */
    uint8_t ladder;          /* H1-H4 bits */
} cu_outputs_t;

typedef struct {
    uint8_t di_fail, out_fail;
    bool di_ok, out_ok;
    uint32_t conflicts; /* times K1 and K2 were requested together */
} safety_t;

/* S1: first action after every start, before UI and network: K1-K8 off, H1-H4 off.
 * Returns the number of failed writes (0 = all confirmed by the HAL). */
int safety_boot_all_off(hal_io_t *io);

void safety_init(safety_t *s);
void safety_note_di(safety_t *s, bool ok);  /* S5 */
void safety_note_out(safety_t *s, bool ok); /* S11 */

/* Last gate before the outputs are written: release per function, measure lock,
 * roof relays never together (S2). Modifies out in place. */
void safety_filter(safety_t *s, const profile_t *p, cu_outputs_t *out);
