#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_lcd_jd9365.h"

/* DSI link + DPI porch timing for one panel variant (private to board). */
typedef struct {
    uint32_t dpi_clock_freq_mhz;
    uint32_t lane_bit_rate_mbps;
    uint32_t hsync_pulse_width;
    uint32_t hsync_back_porch;
    uint32_t hsync_front_porch;
    uint32_t vsync_pulse_width;
    uint32_t vsync_back_porch;
    uint32_t vsync_front_porch;
} board_panel_timing_t;

extern const jd9365_lcd_init_cmd_t board_panel_init_cmds[];
extern const size_t board_panel_init_cmds_len;
extern const board_panel_timing_t board_panel_timing;
