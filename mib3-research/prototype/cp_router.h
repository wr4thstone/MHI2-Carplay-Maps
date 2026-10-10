#ifndef CP_ROUTER_H
#define CP_ROUTER_H

#include <stddef.h>
#include <stdint.h>

/* Research API, NOT the proprietary ScreenStream ABI. No vehicle I/O here.
 * Serialize all calls on one owner thread. Callbacks must not reenter router.
 * Buffers are borrowed only for the duration of each synchronous callback.
 * Sink contexts must remain valid until unbind/reset. */
enum cp_role { CP_MAIN = 0, CP_CLUSTER = 1, CP_ROLE_COUNT = 2 };
enum cp_result {
    CP_OK = 0, CP_INVALID = -1, CP_BUSY = -2, CP_STALE = -3,
    CP_STATE = -4, CP_SINK_FAILED = -5, CP_EXHAUSTED = -6
};

struct cp_display {
    /* Real negotiated values must be supplied by the integration adapter. */
    uint32_t width, height, fps;
};

struct cp_sink {
    /* On open failure, open must release any partially allocated resources.
     * close is called exactly once for every successful open, including
     * configure/submit failure. Each role needs an independent sink context. */
    int (*open)(void *ctx, const struct cp_display *display);
    int (*configure)(void *ctx, const void *avcc, size_t size);
    int (*submit)(void *ctx, const void *data, size_t size, uint64_t timestamp);
    void (*close)(void *ctx);
};

struct cp_slot {
    uint64_t handle;
    struct cp_display display;
    struct cp_sink sink;
    void *ctx;
    unsigned running, configured;
};

struct cp_router {
    uint64_t next_handle;
    struct cp_slot slots[CP_ROLE_COUNT];
};

/* Init once on fresh storage. Reset preserves handle generation, so queued
 * events from an old phone session cannot address a replacement stream. */
void cp_router_init(struct cp_router *router);
int cp_router_bind(struct cp_router *router, enum cp_role role,
                   const struct cp_display *display, const struct cp_sink *sink,
                   void *ctx, uint64_t *handle);
int cp_router_start(struct cp_router *router, uint64_t handle);
int cp_router_configure(struct cp_router *router, uint64_t handle,
                        const void *avcc, size_t size);
int cp_router_submit(struct cp_router *router, uint64_t handle,
                     const void *data, size_t size, uint64_t timestamp);
int cp_router_stop(struct cp_router *router, uint64_t handle);
int cp_router_unbind(struct cp_router *router, uint64_t handle);
void cp_router_reset(struct cp_router *router);

#endif
