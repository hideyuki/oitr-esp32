#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Splits a raw MJPEG stream (plain concatenation of baseline JPEG frames,
 * no container) into individual frames by scanning for SOI (FFD8) and
 * EOI (FFD9). Pure file parsing: no JPEG decoder or LCD dependency.
 */
typedef struct mjpeg_reader mjpeg_reader_t;

/*
 * Open `path` for reading. `frame_buf` (size `frame_buf_size`) is owned by the
 * caller and must outlive the reader; every mjpeg_reader_next() overwrites it.
 */
esp_err_t mjpeg_reader_open(const char *path, uint8_t *frame_buf, size_t frame_buf_size,
                            mjpeg_reader_t **out);

/*
 * Copy the next complete frame (SOI through EOI inclusive) into frame_buf.
 * Returns ESP_ERR_NOT_FOUND at end of file (a trailing partial frame is
 * discarded), ESP_ERR_INVALID_SIZE if the frame does not fit in frame_buf,
 * ESP_FAIL on a read error.
 */
esp_err_t mjpeg_reader_next(mjpeg_reader_t *r, size_t *frame_len);

/* Seek back to the start of the file. */
esp_err_t mjpeg_reader_rewind(mjpeg_reader_t *r);

/* Close the file and free the reader. NULL is ignored. */
void mjpeg_reader_close(mjpeg_reader_t *r);

#ifdef __cplusplus
}
#endif
