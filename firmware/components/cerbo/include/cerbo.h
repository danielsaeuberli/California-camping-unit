/* Victron Cerbo GX over MQTT, read only (SPEC 5): never publishes W/... topics.
 * Feeds energy_t through the logic task. Open: MQTT over the Cerbo access point (O10). */
#pragma once

#include "modules.h"

/* TODO(M4): esp-mqtt client, portal id discovery, keepalive (R/<id>/keepalive). */
int cerbo_start(energy_t *e);
