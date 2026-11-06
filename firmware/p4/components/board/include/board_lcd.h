#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_lcd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Power the MIPI-DSI PHY, create the DSI bus and JD9365 DPI panel
 * (800x1280 native portrait, RGB565, 2 frame buffers), then reset and
 * initialise it with the init sequence of the Kconfig-selected variant.
 */
esp_err_t board_lcd_init(esp_lcd_panel_handle_t *out_panel);

/* Return the two driver-owned DPI frame buffers (PSRAM, RGB565). */
esp_err_t board_lcd_get_frame_buffers(esp_lcd_panel_handle_t panel, void **fb0, void **fb1);

/* LEDC PWM on the backlight pin; starts dark (0 %). */
esp_err_t board_backlight_init(void);

/* Set backlight brightness; values above 100 are clamped to 100. */
esp_err_t board_backlight_set(uint8_t percent);

#ifdef __cplusplus
}
#endif
