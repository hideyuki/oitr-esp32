#pragma once

#include <stddef.h>

#include "esp_err.h"
#include "esp_lcd_types.h"

/*
 * Owns the panel's two DPI frame buffers: hands out the back buffer for the
 * decoder to write a full native-orientation frame into, then flips to it.
 */
typedef struct video_presenter video_presenter_t;

/* Clear both frame buffers to black and make fb0 the visible buffer. */
esp_err_t video_presenter_create(esp_lcd_panel_handle_t panel, void *fb0, void *fb1,
                                 video_presenter_t **out);

/* Buffer not currently scanned out (BOARD_LCD_H_RES x BOARD_LCD_V_RES RGB565). */
void *video_presenter_back_buffer(const video_presenter_t *p);

/* Size in bytes of each frame buffer. */
size_t video_presenter_buffer_size(void);

/* Make the back buffer visible and swap roles. */
esp_err_t video_presenter_flip(video_presenter_t *p);

/* Free the presenter. The frame buffers belong to the panel. NULL is ignored. */
void video_presenter_destroy(video_presenter_t *p);
