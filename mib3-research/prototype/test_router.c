#include "cp_router.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); \
    exit(1); } } while (0)

struct fake_sink {
    unsigned opens, configs, frames, closes;
    int fail_open, fail_config, fail_submit;
    struct cp_display display;
    unsigned char codec[32], frame[32];
    size_t codec_size, frame_size;
    uint64_t timestamp;
};

static int fake_open(void *ctx, const struct cp_display *d)
{
    struct fake_sink *s = ctx;
    if (s->fail_open) return -1;
    s->display = *d;
    ++s->opens;
    return 0;
}
static int fake_config(void *ctx, const void *data, size_t size)
{
    struct fake_sink *s = ctx;
    if (s->fail_config || size > sizeof(s->codec)) return -1;
    memcpy(s->codec, data, size);
    s->codec_size = size;
    ++s->configs;
    return 0;
}
static int fake_submit(void *ctx, const void *data, size_t size, uint64_t ts)
{
    struct fake_sink *s = ctx;
    if (s->fail_submit || size > sizeof(s->frame)) return -1;
    memcpy(s->frame, data, size);
    s->frame_size = size;
    s->timestamp = ts;
    ++s->frames;
    return 0;
}
static void fake_close(void *ctx) { ++((struct fake_sink *)ctx)->closes; }
static const struct cp_sink ops = {
    fake_open, fake_config, fake_submit, fake_close
};
/* Synthetic fixture sizes: these are NOT the Passat cluster dimensions. */
static const struct cp_display main_d = {800, 480, 30};
static const struct cp_display cluster_d = {640, 360, 25};

static void stream_isolation(void)
{
    struct cp_router r;
    struct fake_sink main_s = {0}, cluster_s = {0};
    uint64_t main_h, cluster_h, replacement;
    cp_router_init(&r);
    CHECK(cp_router_bind(&r, CP_MAIN, &main_d, &ops, &main_s, &main_h) == CP_OK);
    CHECK(cp_router_start(&r, main_h) == CP_OK);
    CHECK(cp_router_configure(&r, main_h, "main-codec", 10) == CP_OK);
    CHECK(cp_router_submit(&r, main_h, "main-1", 6, 100) == CP_OK);
    CHECK(cp_router_bind(&r, CP_CLUSTER, &cluster_d, &ops, &main_s,
                         &cluster_h) == CP_BUSY);
    CHECK(cp_router_bind(&r, CP_CLUSTER, &cluster_d, &ops, &cluster_s,
                         &cluster_h) == CP_OK);
    CHECK(cp_router_start(&r, cluster_h) == CP_OK);
    CHECK(cp_router_submit(&r, cluster_h, "map", 3, 110) == CP_STATE);
    CHECK(cp_router_configure(&r, cluster_h, "map-codec", 9) == CP_OK);
    CHECK(cp_router_submit(&r, cluster_h, "map-1", 5, 110) == CP_OK);
    CHECK(cp_router_submit(&r, main_h, "main-2", 6, 120) == CP_OK);
    CHECK(main_s.frames == 2 && cluster_s.frames == 1);
    CHECK(main_s.timestamp == 120 && cluster_s.timestamp == 110);
    CHECK(main_s.frame_size == 6 && memcmp(main_s.frame, "main-2", 6) == 0);
    CHECK(cluster_s.frame_size == 5 && memcmp(cluster_s.frame, "map-1", 5) == 0);
    CHECK(main_s.codec_size == 10 && memcmp(main_s.codec, "main-codec", 10) == 0);
    CHECK(cluster_s.codec_size == 9 && memcmp(cluster_s.codec, "map-codec", 9) == 0);
    CHECK(main_s.display.width == 800 && cluster_s.display.width == 640);
    /* Cluster disconnect must leave main playback running. */
    CHECK(cp_router_unbind(&r, cluster_h) == CP_OK);
    CHECK(cluster_s.closes == 1 && main_s.closes == 0);
    CHECK(cp_router_submit(&r, main_h, "main-3", 6, 130) == CP_OK);
    CHECK(cp_router_bind(&r, CP_CLUSTER, &cluster_d, &ops, &cluster_s,
                         &replacement) == CP_OK);
    CHECK(replacement != cluster_h);
    CHECK(cp_router_submit(&r, cluster_h, "old", 3, 140) == CP_STALE);
    CHECK(cp_router_start(&r, replacement) == CP_OK);
    CHECK(cp_router_configure(&r, replacement, "map-codec", 9) == CP_OK);
    /* Decoder failure closes only cluster; restart needs fresh codec setup. */
    cluster_s.fail_submit = 1;
    CHECK(cp_router_submit(&r, replacement, "bad", 3, 150) == CP_SINK_FAILED);
    CHECK(cluster_s.closes == 2 && main_s.closes == 0);
    CHECK(cp_router_submit(&r, main_h, "main-4", 6, 160) == CP_OK);
    cluster_s.fail_submit = 0;
    CHECK(cp_router_start(&r, replacement) == CP_OK);
    CHECK(cp_router_submit(&r, replacement, "map", 3, 170) == CP_STATE);
    CHECK(cp_router_configure(&r, replacement, "new-codec", 9) == CP_OK);
    CHECK(cp_router_submit(&r, replacement, "map-2", 5, 170) == CP_OK);
    CHECK(cp_router_stop(&r, replacement) == CP_OK);
    CHECK(cp_router_stop(&r, replacement) == CP_OK);
    CHECK(cluster_s.closes == 3);
    cp_router_reset(&r);
    CHECK(main_s.closes == 1 && cluster_s.closes == 3);
    CHECK(cp_router_bind(&r, CP_MAIN, &main_d, &ops, &main_s,
                         &replacement) == CP_OK);
    CHECK(cp_router_start(&r, main_h) == CP_STALE);
    cp_router_reset(&r);
    puts("PASS independent payloads, timestamps, codec state, disconnect and restart");
}

static void errors_and_cleanup(void)
{
    struct cp_router r;
    struct fake_sink s = {0};
    struct cp_sink missing = ops;
    struct cp_display invalid = {0, 1, 1};
    uint64_t h = 0;
    cp_router_init(&r);
    CHECK(cp_router_bind(&r, (enum cp_role)-1, &main_d, &ops, &s, &h) == CP_INVALID);
    CHECK(cp_router_bind(&r, CP_MAIN, &invalid, &ops, &s, &h) == CP_INVALID);
    missing.submit = NULL;
    CHECK(cp_router_bind(&r, CP_MAIN, &main_d, &missing, &s, &h) == CP_INVALID);
    CHECK(cp_router_bind(&r, CP_MAIN, &main_d, &ops, &s, &h) == CP_OK);
    CHECK(cp_router_bind(&r, CP_MAIN, &main_d, &ops, &s, &h) == CP_BUSY);
    CHECK(cp_router_start(&r, 0) == CP_STALE);
    s.fail_open = 1;
    CHECK(cp_router_start(&r, h) == CP_SINK_FAILED);
    CHECK(s.closes == 0);
    s.fail_open = 0;
    CHECK(cp_router_start(&r, h) == CP_OK);
    CHECK(cp_router_start(&r, h) == CP_STATE);
    CHECK(cp_router_configure(&r, h, NULL, 1) == CP_INVALID);
    s.fail_config = 1;
    CHECK(cp_router_configure(&r, h, "x", 1) == CP_SINK_FAILED);
    CHECK(s.closes == 1);
    CHECK(cp_router_submit(&r, h, "x", 1, 0) == CP_STATE);
    CHECK(cp_router_unbind(&r, h) == CP_OK);
    CHECK(s.closes == 1);
    CHECK(cp_router_unbind(&r, h) == CP_STALE);
    /* Never wrap stream IDs into the IDs of an old session. */
    r.next_handle = UINT64_MAX;
    CHECK(cp_router_bind(&r, CP_MAIN, &main_d, &ops, &s, &h) == CP_OK);
    CHECK(h == UINT64_MAX);
    CHECK(cp_router_unbind(&r, h) == CP_OK);
    CHECK(cp_router_bind(&r, CP_MAIN, &main_d, &ops, &s, &h) == CP_EXHAUSTED);
    cp_router_reset(&r);
    CHECK(cp_router_bind(&r, CP_MAIN, &main_d, &ops, &s, &h) == CP_EXHAUSTED);
    puts("PASS invalid inputs, failed opens/configuration, cleanup and stale handles");
}

int main(void)
{
    stream_isolation();
    errors_and_cleanup();
    return 0;
}
