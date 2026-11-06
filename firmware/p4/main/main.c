#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_system.h"

#include "board_lcd.h"
#include "board_pins.h"
#include "board_sdcard.h"
#include "video_player.h"

#define SD_MOUNT_POINT        "/sdcard"
#define VIDEO_PATH            SD_MOUNT_POINT "/elis.mjpeg"
#define VIDEO_FPS             30
#define BACKLIGHT_ON_PERCENT  100
#define RGB565_DARK_BLUE      0x0010U   /* "playback could not start" marker */

static const char *TAG = "oitr";

/* Fill a whole frame buffer with one colour and make it visible. */
static void show_solid_color(esp_lcd_panel_handle_t panel, void *fb, uint16_t color)
{
    uint16_t *px = fb;
    const size_t count = (size_t)BOARD_LCD_H_RES * BOARD_LCD_V_RES;
    for (size_t i = 0; i < count; i++) {
        px[i] = color;
    }
    esp_err_t err = esp_lcd_panel_draw_bitmap(panel, 0, 0, BOARD_LCD_H_RES, BOARD_LCD_V_RES, fb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "draw fallback screen failed: %s", esp_err_to_name(err));
    }
}

static esp_err_t start_playback(esp_lcd_panel_handle_t panel, void *fb0, void *fb1)
{
    esp_err_t err = board_sdcard_mount(SD_MOUNT_POINT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SD card mount failed: %s", esp_err_to_name(err));
        return err;
    }
    const video_player_config_t cfg = {
        .path = VIDEO_PATH,
        .panel = panel,
        .fb = { fb0, fb1 },
        .fps = VIDEO_FPS,
        .loop = true,
    };
    err = video_player_start(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "video player start failed: %s", esp_err_to_name(err));
    }
    return err;
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP-IDF %s, free internal %u, free PSRAM %u", esp_get_idf_version(),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    ESP_ERROR_CHECK(board_backlight_init());

    esp_lcd_panel_handle_t panel = NULL;
    ESP_ERROR_CHECK(board_lcd_init(&panel));

    void *fb0 = NULL;
    void *fb1 = NULL;
    ESP_ERROR_CHECK(board_lcd_get_frame_buffers(panel, &fb0, &fb1));

    if (start_playback(panel, fb0, fb1) != ESP_OK) {
        show_solid_color(panel, fb0, RGB565_DARK_BLUE);
    }
    ESP_ERROR_CHECK(board_backlight_set(BACKLIGHT_ON_PERCENT));
}
