#include "state_bus.h"

#include <string.h>

static void lock(state_bus_t *b)
{
    if (b->lock) {
        b->lock(b->lock_ctx);
    }
}

static void unlock(state_bus_t *b)
{
    if (b->unlock) {
        b->unlock(b->lock_ctx);
    }
}

void state_bus_init(state_bus_t *b)
{
    memset(b, 0, sizeof *b);
}

void state_bus_publish(state_bus_t *b, const cu_state_t *s)
{
    lock(b);
    uint32_t v = b->state.version + 1;
    b->state = *s;
    b->state.version = v;
    unlock(b);
}

void state_bus_snapshot(state_bus_t *b, cu_state_t *out)
{
    lock(b);
    *out = b->state;
    unlock(b);
}

bool state_bus_push(state_bus_t *b, const cu_cmd_t *c)
{
    lock(b);
    uint8_t next = (uint8_t)((b->head + 1) % STATE_BUS_QUEUE);
    bool ok = next != b->tail;
    if (ok) {
        b->q[b->head] = *c;
        b->head = next;
    }
    unlock(b);
    return ok;
}

bool state_bus_pop(state_bus_t *b, cu_cmd_t *out)
{
    lock(b);
    bool ok = b->tail != b->head;
    if (ok) {
        *out = b->q[b->tail];
        b->tail = (uint8_t)((b->tail + 1) % STATE_BUS_QUEUE);
    }
    unlock(b);
    return ok;
}
