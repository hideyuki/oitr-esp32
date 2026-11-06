#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Per-window playback statistics, logged every VIDEO_STATS_WINDOW frames. */
#define VIDEO_STATS_WINDOW  90

typedef struct {
    int64_t window_start_us;
    uint32_t frames;
    uint32_t late;
    int64_t read_us;
    int64_t decode_us;
    int64_t flip_us;
} video_stats_t;

typedef struct {
    int64_t read_us;
    int64_t decode_us;
    int64_t flip_us;
    bool late;
} video_frame_timing_t;

/* Start a fresh window at `now_us`. */
video_stats_t video_stats_begin(int64_t now_us);

/* Account one frame; logs and returns a fresh window when the window is full. */
video_stats_t video_stats_add(video_stats_t s, const video_frame_timing_t *t, int64_t now_us);
