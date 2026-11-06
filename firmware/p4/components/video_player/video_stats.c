#include "video_stats.h"

#include <inttypes.h>

#include "esp_log.h"

static const char *TAG = "video_stats";

video_stats_t video_stats_begin(int64_t now_us)
{
    return (video_stats_t){ .window_start_us = now_us };
}

static void log_window(const video_stats_t *s, int64_t now_us)
{
    const int64_t elapsed_us = now_us - s->window_start_us;
    const uint32_t n = s->frames;
    /* fps with two decimals, computed in integers */
    const int64_t fps_x100 = (elapsed_us > 0) ? ((int64_t)n * 100000000LL) / elapsed_us : 0;

    ESP_LOGI(TAG, "%" PRIu32 " frames: %" PRId64 ".%02" PRId64 " fps, avg read %" PRId64
             " us, decode %" PRId64 " us, flip %" PRId64 " us, late %" PRIu32,
             n, fps_x100 / 100, fps_x100 % 100,
             s->read_us / n, s->decode_us / n, s->flip_us / n, s->late);
}

video_stats_t video_stats_add(video_stats_t s, const video_frame_timing_t *t, int64_t now_us)
{
    const video_stats_t next = {
        .window_start_us = s.window_start_us,
        .frames = s.frames + 1,
        .late = s.late + (t->late ? 1U : 0U),
        .read_us = s.read_us + t->read_us,
        .decode_us = s.decode_us + t->decode_us,
        .flip_us = s.flip_us + t->flip_us,
    };
    if (next.frames < VIDEO_STATS_WINDOW) {
        return next;
    }
    log_window(&next, now_us);
    return video_stats_begin(now_us);
}
