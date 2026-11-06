#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_system.h"
#include "sdkconfig.h"

#include "board_lcd.h"
#include "board_pins.h"

#define RGB565_RED    0xF800U
#define RGB565_GREEN  0x07E0U
#define RGB565_BLUE   0x001FU

#if CONFIG_BOARD_PANEL_OLD
#define PANEL_VARIANT_NAME "old"
#else
#define PANEL_VARIANT_NAME "new"
#endif

static const char *TAG = "oitr";

/* Throwaway test pattern: red / green / blue horizontal thirds (native portrait). */
static void fill_rgb_bands(uint16_t *fb, int width, int height)
{
    const int band_h = height / 3;
    for (int y = 0; y < height; y++) {
        const uint16_t color = (y < band_h) ? RGB565_RED
                             : (y < 2 * band_h) ? RGB565_GREEN
                             : RGB565_BLUE;
        uint16_t *row = fb + (size_t)y * (size_t)width;
        for (int x = 0; x < width; x++) {
            row[x] = color;
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP-IDF %s", esp_get_idf_version());
    ESP_LOGI(TAG, "Free internal heap: %u bytes",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGI(TAG, "Free PSRAM: %u bytes",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    ESP_LOGI(TAG, "LCD: RST=%d BL=%d native=%dx%d logical=%dx%d",
             BOARD_LCD_RST, BOARD_LCD_BACKLIGHT,
             BOARD_LCD_H_RES, BOARD_LCD_V_RES,
             BOARD_DISPLAY_W, BOARD_DISPLAY_H);
    ESP_LOGI(TAG, "SD: CLK=%d CMD=%d D0=%d D1=%d D2=%d D3=%d",
             BOARD_SD_CLK, BOARD_SD_CMD,
             BOARD_SD_D0, BOARD_SD_D1, BOARD_SD_D2, BOARD_SD_D3);

    ESP_ERROR_CHECK(board_backlight_init());

    esp_lcd_panel_handle_t panel = NULL;
    ESP_ERROR_CHECK(board_lcd_init(&panel));

    void *fb0 = NULL;
    void *fb1 = NULL;
    ESP_ERROR_CHECK(board_lcd_get_frame_buffers(panel, &fb0, &fb1));

    fill_rgb_bands((uint16_t *)fb0, BOARD_LCD_H_RES, BOARD_LCD_V_RES);
    /* fb0 is a driver frame buffer, so draw_bitmap does no copy: it only runs
     * esp_cache_msync(C2M) over the updated lines and makes fb0 current. */
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, 0, BOARD_LCD_H_RES, BOARD_LCD_V_RES, fb0));

    ESP_ERROR_CHECK(board_backlight_set(100));
    ESP_LOGI(TAG, "LCD test pattern shown (panel variant: %s)", PANEL_VARIANT_NAME);
}
