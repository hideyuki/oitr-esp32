#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

/* Hardware JPEG decoder wrapper: one engine and one compressed-input buffer. */
typedef struct video_decoder video_decoder_t;

/* Create the decoder engine and a DMA-capable input buffer of `in_size` bytes. */
esp_err_t video_decoder_create(size_t in_size, video_decoder_t **out);

/* Compressed-input buffer to fill (e.g. by mjpeg_reader) and its usable size. */
uint8_t *video_decoder_input(const video_decoder_t *d, size_t *size);

/* Read the picture dimensions of the JPEG currently in the input buffer. */
esp_err_t video_decoder_probe(const video_decoder_t *d, size_t len, uint32_t *width, uint32_t *height);

/*
 * Decode `len` bytes from the input buffer as RGB565 into `dst`.
 * `dst` must be DMA-capable and cache-line aligned in both address and
 * `dst_size` (a DPI frame buffer or jpeg_alloc_decoder_mem output qualifies),
 * and large enough for the decoded picture.
 */
esp_err_t video_decoder_decode(video_decoder_t *d, size_t len, void *dst, size_t dst_size);

/* Delete the engine and free the input buffer. NULL is ignored. */
void video_decoder_destroy(video_decoder_t *d);
