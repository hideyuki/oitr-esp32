#include "video_player.h"

#include <inttypes.h>
#include <stdatomic.h>
#include <stdlib.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "board_pins.h"
#include "mjpeg_reader.h"
#include "video_decode.h"
#include "video_present.h"
#include "video_stats.h"

#define PLAYER_TASK_NAME        "video_player"
#define PLAYER_TASK_STACK       (8 * 1024)
#define PLAYER_TASK_PRIORITY    5
#define PLAYER_TASK_CORE        1
#define JPEG_IN_BUF_SIZE        (256 * 1024)
#define FPS_MAX                 120U
#define US_PER_SECOND           1000000LL
#define US_PER_MS               1000LL

static const char *TAG = "video_player";

typedef struct {
    mjpeg_reader_t *reader;
    video_decoder_t *decoder;
    video_presenter_t *presenter;
    int64_t period_us;
    bool loop;
} session_t;

static struct {
    TaskHandle_t task;
    SemaphoreHandle_t done;     /* given by the task right before it deletes itself */
    atomic_bool stop_requested;
    session_t session;
} s_player;

static void session_destroy(session_t *ss)
{
    mjpeg_reader_close(ss->reader);
    video_presenter_destroy(ss->presenter);
    video_decoder_destroy(ss->decoder);
    *ss = (session_t){ 0 };
}

static esp_err_t session_create(const video_player_config_t *cfg, session_t *ss)
{
    session_t s = {
        .period_us = US_PER_SECOND / cfg->fps,
        .loop = cfg->loop,
    };
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_ERROR(video_decoder_create(JPEG_IN_BUF_SIZE, &s.decoder), err, TAG,
                      "decoder setup failed");
    size_t in_size = 0;
    uint8_t *in_buf = video_decoder_input(s.decoder, &in_size);
    ESP_GOTO_ON_ERROR(mjpeg_reader_open(cfg->path, in_buf, in_size, &s.reader), err, TAG,
                      "cannot open %s", cfg->path);
    ESP_GOTO_ON_ERROR(video_presenter_create(cfg->panel, cfg->fb[0], cfg->fb[1], &s.presenter), err, TAG,
                      "presenter setup failed");
    *ss = s;
    return ESP_OK;

err:
    session_destroy(&s);
    return ret;
}

/* Next frame into the decoder input; rewinds at EOF when looping. ESP_ERR_NOT_FOUND = finished. */
static esp_err_t read_frame(const session_t *ss, size_t *len)
{
    esp_err_t err = mjpeg_reader_next(ss->reader, len);
    if (err != ESP_ERR_NOT_FOUND || !ss->loop) {
        return err;
    }
    ESP_RETURN_ON_ERROR(mjpeg_reader_rewind(ss->reader), TAG, "rewind failed");
    err = mjpeg_reader_next(ss->reader, len);
    if (err == ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "file contains no complete JPEG frame");
        return ESP_ERR_INVALID_STATE;
    }
    return err;
}

static esp_err_t check_first_frame(const session_t *ss, size_t len)
{
    uint32_t w = 0;
    uint32_t h = 0;
    ESP_RETURN_ON_ERROR(video_decoder_probe(ss->decoder, len, &w, &h), TAG, "first frame unreadable");
    ESP_LOGI(TAG, "first frame: %" PRIu32 "x%" PRIu32 ", %u bytes", w, h, (unsigned)len);
    ESP_RETURN_ON_FALSE(w == BOARD_LCD_H_RES && h == BOARD_LCD_V_RES, ESP_ERR_INVALID_SIZE, TAG,
                        "unsupported frame size %" PRIu32 "x%" PRIu32 " (need pre-rotated portrait %dx%d;"
                        " convert with tools/convert_video.sh)",
                        w, h, BOARD_LCD_H_RES, BOARD_LCD_V_RES);
    return ESP_OK;
}

/* Sleep until the absolute deadline. Returns true if it had already passed. */
static bool wait_until(int64_t deadline_us)
{
    const int64_t remaining_us = deadline_us - esp_timer_get_time();
    if (remaining_us <= 0) {
        return true;
    }
    const TickType_t ticks = pdMS_TO_TICKS(remaining_us / US_PER_MS);
    if (ticks > 0) {
        vTaskDelay(ticks);
    }
    return false;
}

/* Read one frame, decode it straight into the back frame buffer and flip, filling in its timing. */
static esp_err_t play_frame(const session_t *ss, bool first, video_frame_timing_t *t)
{
    size_t len = 0;
    const int64_t t0 = esp_timer_get_time();
    esp_err_t err = read_frame(ss, &len);
    if (err != ESP_OK) {
        return err;
    }
    if (first) {
        ESP_RETURN_ON_ERROR(check_first_frame(ss, len), TAG, "first frame rejected");
    }
    const int64_t t1 = esp_timer_get_time();
    ESP_RETURN_ON_ERROR(video_decoder_decode(ss->decoder, len, video_presenter_back_buffer(ss->presenter),
                                             video_presenter_buffer_size()),
                        TAG, "decode failed");
    const int64_t t2 = esp_timer_get_time();
    ESP_RETURN_ON_ERROR(video_presenter_flip(ss->presenter), TAG, "flip failed");
    const int64_t t3 = esp_timer_get_time();

    *t = (video_frame_timing_t){
        .read_us = t1 - t0,
        .decode_us = t2 - t1,
        .flip_us = t3 - t2,
    };
    return ESP_OK;
}

static esp_err_t play(const session_t *ss)
{
    const int64_t start_us = esp_timer_get_time();
    int64_t deadline_us = start_us + ss->period_us;
    video_stats_t stats = video_stats_begin(start_us);
    bool first = true;

    while (!atomic_load(&s_player.stop_requested)) {
        video_frame_timing_t timing = { 0 };
        esp_err_t err = play_frame(ss, first, &timing);
        if (err == ESP_ERR_NOT_FOUND) {
            ESP_LOGI(TAG, "end of file");
            return ESP_OK;
        }
        ESP_RETURN_ON_ERROR(err, TAG, "playback aborted");
        first = false;

        timing.late = wait_until(deadline_us);
        deadline_us += ss->period_us;
        stats = video_stats_add(stats, &timing, esp_timer_get_time());
    }
    ESP_LOGI(TAG, "stop requested");
    return ESP_OK;
}

static void player_task(void *arg)
{
    session_t *ss = arg;
    esp_err_t err = play(ss);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "player stopped: %s", esp_err_to_name(err));
    }
    session_destroy(ss);
    xSemaphoreGive(s_player.done);
    vTaskDelete(NULL);
}

/* Join a task that has already finished on its own. True if the player is idle afterwards. */
static bool reap_finished_task(void)
{
    if (s_player.task == NULL) {
        return true;
    }
    if (xSemaphoreTake(s_player.done, 0) != pdTRUE) {
        return false;
    }
    s_player.task = NULL;
    return true;
}

static esp_err_t validate_config(const video_player_config_t *cfg)
{
    ESP_RETURN_ON_FALSE(cfg != NULL && cfg->path != NULL && cfg->panel != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "missing path or panel");
    ESP_RETURN_ON_FALSE(cfg->fb[0] != NULL && cfg->fb[1] != NULL && cfg->fb[0] != cfg->fb[1],
                        ESP_ERR_INVALID_ARG, TAG, "need two distinct frame buffers");
    ESP_RETURN_ON_FALSE(cfg->fps > 0 && cfg->fps <= FPS_MAX, ESP_ERR_INVALID_ARG, TAG,
                        "fps must be 1..%u", FPS_MAX);
    return ESP_OK;
}

esp_err_t video_player_start(const video_player_config_t *cfg)
{
    ESP_RETURN_ON_ERROR(validate_config(cfg), TAG, "invalid config");
    ESP_RETURN_ON_FALSE(reap_finished_task(), ESP_ERR_INVALID_STATE, TAG, "player already running");

    if (s_player.done == NULL) {
        s_player.done = xSemaphoreCreateBinary();
        ESP_RETURN_ON_FALSE(s_player.done != NULL, ESP_ERR_NO_MEM, TAG, "no memory for semaphore");
    }
    ESP_RETURN_ON_ERROR(session_create(cfg, &s_player.session), TAG, "player setup failed");

    atomic_store(&s_player.stop_requested, false);
    const BaseType_t ok = xTaskCreatePinnedToCore(player_task, PLAYER_TASK_NAME, PLAYER_TASK_STACK,
                                                  &s_player.session, PLAYER_TASK_PRIORITY,
                                                  &s_player.task, PLAYER_TASK_CORE);
    if (ok != pdPASS) {
        s_player.task = NULL;
        session_destroy(&s_player.session);
        ESP_LOGE(TAG, "task creation failed");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "playing %s at %" PRIu32 " fps%s", cfg->path, cfg->fps, cfg->loop ? " (loop)" : "");
    return ESP_OK;
}

void video_player_stop(void)
{
    if (s_player.task == NULL) {
        return;
    }
    atomic_store(&s_player.stop_requested, true);
    /* Bounded: the loop checks the flag every frame and each step has its own timeout. */
    xSemaphoreTake(s_player.done, portMAX_DELAY);
    s_player.task = NULL;
}
