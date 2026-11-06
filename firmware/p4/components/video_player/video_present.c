#include "video_present.h"

#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"

#include "board_pins.h"

#define FB_BYTES_PER_PIXEL  2
#define FB_SIZE_BYTES       ((size_t)BOARD_LCD_H_RES * BOARD_LCD_V_RES * FB_BYTES_PER_PIXEL)
#define FB_COUNT            2

static const char *TAG = "video_present";

struct video_presenter {
    esp_lcd_panel_handle_t panel;
    void *fb[FB_COUNT];
    int back;             /* index of the buffer not currently scanned out */
};

static esp_err_t show_buffer(const video_presenter_t *p, int index)
{
    /* fb is a driver frame buffer: draw_bitmap only syncs the cache and switches to it. */
    return esp_lcd_panel_draw_bitmap(p->panel, 0, 0, BOARD_LCD_H_RES, BOARD_LCD_V_RES, p->fb[index]);
}

static esp_err_t clear_buffers(const video_presenter_t *p)
{
    /* Draw fb1 first so fb0 ends up visible and fb1 is the first back buffer. */
    for (int i = FB_COUNT - 1; i >= 0; i--) {
        memset(p->fb[i], 0, FB_SIZE_BYTES);
        ESP_RETURN_ON_ERROR(show_buffer(p, i), TAG, "clear fb%d failed", i);
    }
    return ESP_OK;
}

esp_err_t video_presenter_create(esp_lcd_panel_handle_t panel, void *fb0, void *fb1,
                                 video_presenter_t **out)
{
    ESP_RETURN_ON_FALSE(panel != NULL && fb0 != NULL && fb1 != NULL && out != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    video_presenter_t *p = calloc(1, sizeof(*p));
    ESP_RETURN_ON_FALSE(p != NULL, ESP_ERR_NO_MEM, TAG, "no memory for presenter");
    p->panel = panel;
    p->fb[0] = fb0;
    p->fb[1] = fb1;
    p->back = 1;

    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_ERROR(clear_buffers(p), err, TAG, "frame buffer clear failed");

    *out = p;
    return ESP_OK;

err:
    video_presenter_destroy(p);
    return ret;
}

void *video_presenter_back_buffer(const video_presenter_t *p)
{
    return (p != NULL) ? p->fb[p->back] : NULL;
}

size_t video_presenter_buffer_size(void)
{
    return FB_SIZE_BYTES;
}

esp_err_t video_presenter_flip(video_presenter_t *p)
{
    ESP_RETURN_ON_FALSE(p != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_ERROR(show_buffer(p, p->back), TAG, "frame buffer flip failed");
    p->back ^= 1;
    return ESP_OK;
}

void video_presenter_destroy(video_presenter_t *p)
{
    free(p);
}
