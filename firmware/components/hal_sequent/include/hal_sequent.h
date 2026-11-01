/* hal_io_t for SM-H-001 (relays, AI) and SM-I-009 (DI, edge counters) on I2C 0 (SPEC 4.1, 4.2).
 * M1: stub that reports every access as failed. Device code follows in M4 when the cards
 * are on the bench (CLAUDE.md). Open: SM-H-001 address, 3.3 V from the header (O17). */
#pragma once

#include "hal_io.h"

hal_io_t *hal_sequent_io(void);
