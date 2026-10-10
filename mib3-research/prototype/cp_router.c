#include "cp_router.h"
#include <string.h>

static struct cp_slot *find(struct cp_router *router, uint64_t handle)
{
    size_t i;
    if (!router || !handle) return NULL;
    for (i = 0; i < CP_ROLE_COUNT; ++i)
        if (router->slots[i].handle == handle) return &router->slots[i];
    return NULL;
}

static void stop_slot(struct cp_slot *slot)
{
    if (slot->running) {
        slot->running = 0;
        slot->configured = 0;
        slot->sink.close(slot->ctx);
    }
}

void cp_router_init(struct cp_router *router)
{
    if (!router) return;
    memset(router, 0, sizeof(*router));
    router->next_handle = 1;
}

int cp_router_bind(struct cp_router *router, enum cp_role role,
                   const struct cp_display *display, const struct cp_sink *sink,
                   void *ctx, uint64_t *handle)
{
    struct cp_slot *slot;
    size_t i;
    if (!router || (unsigned)role >= CP_ROLE_COUNT || !display || !sink ||
        !ctx || !handle || !display->width || !display->height || !display->fps ||
        !sink->open || !sink->configure || !sink->submit || !sink->close)
        return CP_INVALID;
    slot = &router->slots[role];
    if (slot->handle) return CP_BUSY;
    /* Prevent an accidental shared decoder context between the two roles. */
    for (i = 0; i < CP_ROLE_COUNT; ++i)
        if (router->slots[i].handle && router->slots[i].ctx == ctx)
            return CP_BUSY;
    if (!router->next_handle) return CP_EXHAUSTED;
    slot->display = *display;
    slot->sink = *sink;
    slot->ctx = ctx;
    slot->handle = router->next_handle++;
    *handle = slot->handle;
    return CP_OK;
}

int cp_router_start(struct cp_router *router, uint64_t handle)
{
    struct cp_slot *slot = find(router, handle);
    if (!slot) return CP_STALE;
    if (slot->running) return CP_STATE;
    if (slot->sink.open(slot->ctx, &slot->display)) return CP_SINK_FAILED;
    slot->running = 1;
    slot->configured = 0;
    return CP_OK;
}

int cp_router_configure(struct cp_router *router, uint64_t handle,
                        const void *avcc, size_t size)
{
    struct cp_slot *slot = find(router, handle);
    if (!slot) return CP_STALE;
    if (!avcc || !size) return CP_INVALID;
    if (!slot->running) return CP_STATE;
    /* Codec validation/ownership belongs to the sink, not this dispatcher. */
    if (slot->sink.configure(slot->ctx, avcc, size)) {
        stop_slot(slot);
        return CP_SINK_FAILED;
    }
    slot->configured = 1;
    return CP_OK;
}

int cp_router_submit(struct cp_router *router, uint64_t handle,
                     const void *data, size_t size, uint64_t timestamp)
{
    struct cp_slot *slot = find(router, handle);
    if (!slot) return CP_STALE;
    if (!data || !size) return CP_INVALID;
    if (!slot->running || !slot->configured) return CP_STATE;
    if (slot->sink.submit(slot->ctx, data, size, timestamp)) {
        stop_slot(slot);
        return CP_SINK_FAILED;
    }
    return CP_OK;
}

int cp_router_stop(struct cp_router *router, uint64_t handle)
{
    struct cp_slot *slot = find(router, handle);
    if (!slot) return CP_STALE;
    stop_slot(slot);
    return CP_OK;
}

int cp_router_unbind(struct cp_router *router, uint64_t handle)
{
    struct cp_slot *slot = find(router, handle);
    if (!slot) return CP_STALE;
    stop_slot(slot);
    memset(slot, 0, sizeof(*slot));
    return CP_OK;
}

void cp_router_reset(struct cp_router *router)
{
    size_t i;
    if (!router) return;
    for (i = 0; i < CP_ROLE_COUNT; ++i) {
        stop_slot(&router->slots[i]);
        memset(&router->slots[i], 0, sizeof(router->slots[i]));
    }
}
