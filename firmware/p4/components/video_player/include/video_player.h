#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_lcd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *path;               /* raw MJPEG file; must stay valid while playing */
    esp_lcd_panel_handle_t panel;   /* DPI panel owning fb[] */
    void *fb[2];                    /* the panel's two frame buffers */
    uint32_t fps;                   /* playback rate, 1..120 */
    bool loop;                      /* rewind at end of file instead of stopping */
} video_player_config_t;

/*
 * Open the file (pre-rotated 800x1280 MJPEG, see tools/convert_video.sh),
 * set up the JPEG decoder, clear the frame buffers
 * and start the playback task (core 1). Returns once the task is running;
 * errors found later (bad frame size, read/decode failure) are logged and
 * end the task. ESP_ERR_INVALID_STATE if a player is still running.
 */
esp_err_t video_player_start(const video_player_config_t *cfg);

/* Ask the playback task to finish, wait for it and free everything. Safe to call when idle. */
void video_player_stop(void);

#ifdef __cplusplus
}
#endif
