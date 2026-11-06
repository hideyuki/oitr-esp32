#include "mjpeg_reader.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"

#define READ_BUF_SIZE    (64 * 1024)
#define READ_BUF_ALIGN   64          /* cache line, lets the SD driver DMA straight in */
#define JPEG_MARKER      0xFFU
#define JPEG_SOI         0xD8U
#define JPEG_EOI         0xD9U

static const char *TAG = "mjpeg_reader";

struct mjpeg_reader {
    FILE *fp;
    uint8_t *rbuf;        /* internal read buffer (PSRAM) */
    size_t rlen;          /* valid bytes in rbuf */
    size_t rpos;          /* next unread byte in rbuf */
    uint8_t *frame;       /* caller-owned output buffer */
    size_t frame_cap;
};

/* Scan state for one mjpeg_reader_next() call. */
typedef struct {
    bool in_frame;        /* SOI seen, copying bytes into the frame */
    bool prev_ff;         /* last consumed byte was 0xFF (may span refills) */
    size_t len;           /* bytes copied into the frame so far */
} scan_state_t;

/* Make sure at least one unread byte is buffered. ESP_ERR_NOT_FOUND at EOF. */
static esp_err_t refill(mjpeg_reader_t *r)
{
    if (r->rpos < r->rlen) {
        return ESP_OK;
    }
    const size_t n = fread(r->rbuf, 1, READ_BUF_SIZE, r->fp);
    r->rpos = 0;
    r->rlen = n;
    if (n > 0) {
        return ESP_OK;
    }
    if (ferror(r->fp)) {
        ESP_LOGE(TAG, "read error");
        return ESP_FAIL;
    }
    return ESP_ERR_NOT_FOUND;
}

static esp_err_t append(mjpeg_reader_t *r, scan_state_t *st, const uint8_t *src, size_t n)
{
    if (st->len + n > r->frame_cap) {
        ESP_LOGE(TAG, "frame larger than %u bytes", (unsigned)r->frame_cap);
        return ESP_ERR_INVALID_SIZE;
    }
    memcpy(r->frame + st->len, src, n);
    st->len += n;
    return ESP_OK;
}

/* Handle the byte right after a 0xFF. Returns true when the frame is complete. */
static esp_err_t handle_marker_byte(mjpeg_reader_t *r, scan_state_t *st, uint8_t c, bool *done)
{
    st->prev_ff = (c == JPEG_MARKER);
    if (!st->in_frame) {
        if (c == JPEG_SOI) {
            static const uint8_t soi[2] = { JPEG_MARKER, JPEG_SOI };
            st->in_frame = true;
            st->len = 0;
            return append(r, st, soi, sizeof(soi));
        }
        return ESP_OK;
    }
    esp_err_t err = append(r, st, &c, 1);
    *done = (err == ESP_OK) && (c == JPEG_EOI);
    return err;
}

/*
 * Consume buffered bytes up to and including the next 0xFF (or the whole
 * buffer if there is none), copying them when inside a frame.
 */
static esp_err_t consume_span(mjpeg_reader_t *r, scan_state_t *st)
{
    const uint8_t *p = r->rbuf + r->rpos;
    const size_t avail = r->rlen - r->rpos;
    const uint8_t *ff = memchr(p, JPEG_MARKER, avail);
    const size_t n = (ff != NULL) ? (size_t)(ff - p) + 1 : avail;

    if (st->in_frame) {
        esp_err_t err = append(r, st, p, n);
        if (err != ESP_OK) {
            return err;
        }
    }
    r->rpos += n;
    st->prev_ff = (ff != NULL);
    return ESP_OK;
}

esp_err_t mjpeg_reader_next(mjpeg_reader_t *r, size_t *frame_len)
{
    if (r == NULL || frame_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    scan_state_t st = { 0 };
    bool done = false;

    while (!done) {
        esp_err_t err = refill(r);
        if (err != ESP_OK) {
            return err;
        }
        if (st.prev_ff) {
            const uint8_t c = r->rbuf[r->rpos++];
            err = handle_marker_byte(r, &st, c, &done);
        } else {
            err = consume_span(r, &st);
        }
        if (err != ESP_OK) {
            return err;
        }
    }
    *frame_len = st.len;
    return ESP_OK;
}

esp_err_t mjpeg_reader_open(const char *path, uint8_t *frame_buf, size_t frame_buf_size,
                            mjpeg_reader_t **out)
{
    if (path == NULL || frame_buf == NULL || frame_buf_size < 4 || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    mjpeg_reader_t *r = calloc(1, sizeof(*r));
    if (r == NULL) {
        return ESP_ERR_NO_MEM;
    }
    r->rbuf = heap_caps_aligned_alloc(READ_BUF_ALIGN, READ_BUF_SIZE, MALLOC_CAP_SPIRAM);
    if (r->rbuf == NULL) {
        free(r);
        return ESP_ERR_NO_MEM;
    }
    r->fp = fopen(path, "rb");
    if (r->fp == NULL) {
        ESP_LOGE(TAG, "cannot open %s", path);
        heap_caps_free(r->rbuf);
        free(r);
        return ESP_ERR_NOT_FOUND;
    }
    /* We already read in 64 KB chunks; a second stdio buffer would only add a copy. */
    setvbuf(r->fp, NULL, _IONBF, 0);
    r->frame = frame_buf;
    r->frame_cap = frame_buf_size;
    *out = r;
    return ESP_OK;
}

esp_err_t mjpeg_reader_rewind(mjpeg_reader_t *r)
{
    if (r == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (fseek(r->fp, 0, SEEK_SET) != 0) {
        ESP_LOGE(TAG, "seek failed");
        return ESP_FAIL;
    }
    clearerr(r->fp);
    r->rlen = 0;
    r->rpos = 0;
    return ESP_OK;
}

void mjpeg_reader_close(mjpeg_reader_t *r)
{
    if (r == NULL) {
        return;
    }
    fclose(r->fp);
    heap_caps_free(r->rbuf);
    free(r);
}
