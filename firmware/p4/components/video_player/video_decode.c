#include "video_decode.h"

#include <stdlib.h>

#include "driver/jpeg_decode.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#define DECODE_TIMEOUT_MS   100

/*
 * In esp_driver_jpeg, JPEG_DEC_RGB_ELEMENT_ORDER_RGB byte-swaps RGB565 into
 * big-endian; BGR yields the CPU-native little-endian RGB565 that the DPI
 * frame buffer (and the IDF jpeg example / vendor player) use.
 */
#define DECODE_RGB_ORDER    JPEG_DEC_RGB_ELEMENT_ORDER_BGR

static const char *TAG = "video_decode";

struct video_decoder {
    jpeg_decoder_handle_t engine;
    uint8_t *in_buf;
    size_t in_size;
};

esp_err_t video_decoder_create(size_t in_size, video_decoder_t **out)
{
    ESP_RETURN_ON_FALSE(in_size > 0 && out != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    video_decoder_t *d = calloc(1, sizeof(*d));
    ESP_RETURN_ON_FALSE(d != NULL, ESP_ERR_NO_MEM, TAG, "no memory for decoder");

    esp_err_t ret = ESP_OK;
    const jpeg_decode_engine_cfg_t eng_cfg = {
        .intr_priority = 0,
        .timeout_ms = DECODE_TIMEOUT_MS,
    };
    ESP_GOTO_ON_ERROR(jpeg_new_decoder_engine(&eng_cfg, &d->engine), err, TAG, "new JPEG engine failed");

    const jpeg_decode_memory_alloc_cfg_t mem_cfg = { .buffer_direction = JPEG_DEC_ALLOC_INPUT_BUFFER };
    d->in_buf = jpeg_alloc_decoder_mem(in_size, &mem_cfg, &d->in_size);
    ESP_GOTO_ON_FALSE(d->in_buf != NULL, ESP_ERR_NO_MEM, err, TAG, "no memory for JPEG input");

    *out = d;
    return ESP_OK;

err:
    video_decoder_destroy(d);
    return ret;
}

uint8_t *video_decoder_input(const video_decoder_t *d, size_t *size)
{
    if (size != NULL) {
        *size = d->in_size;
    }
    return d->in_buf;
}

esp_err_t video_decoder_probe(const video_decoder_t *d, size_t len, uint32_t *width, uint32_t *height)
{
    jpeg_decode_picture_info_t info = { 0 };
    ESP_RETURN_ON_ERROR(jpeg_decoder_get_info(d->in_buf, (uint32_t)len, &info), TAG, "JPEG header parse failed");
    *width = info.width;
    *height = info.height;
    return ESP_OK;
}

esp_err_t video_decoder_decode(video_decoder_t *d, size_t len, void *dst, size_t dst_size)
{
    ESP_RETURN_ON_FALSE(d != NULL && dst != NULL && dst_size > 0, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    const jpeg_decode_cfg_t cfg = {
        .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
        .rgb_order = DECODE_RGB_ORDER,
        .conv_std = JPEG_YUV_RGB_CONV_STD_BT601,
    };
    uint32_t written = 0;
    ESP_RETURN_ON_ERROR(jpeg_decoder_process(d->engine, &cfg, d->in_buf, (uint32_t)len,
                                             dst, (uint32_t)dst_size, &written),
                        TAG, "JPEG decode failed");
    return ESP_OK;
}

void video_decoder_destroy(video_decoder_t *d)
{
    if (d == NULL) {
        return;
    }
    if (d->engine != NULL) {
        jpeg_del_decoder_engine(d->engine);
    }
    heap_caps_free(d->in_buf);
    free(d);
}
