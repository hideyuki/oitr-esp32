#include "driver/ledc.h"
#include "esp_check.h"

#include "board_lcd.h"
#include "board_pins.h"

#define BL_LEDC_MODE        LEDC_LOW_SPEED_MODE
#define BL_LEDC_TIMER       LEDC_TIMER_1
#define BL_LEDC_CHANNEL     LEDC_CHANNEL_0
#define BL_LEDC_RESOLUTION  LEDC_TIMER_10_BIT
#define BL_LEDC_FREQ_HZ     5000
#define BL_DUTY_MAX         ((1U << 10) - 1U)
#define BL_PERCENT_MAX      100U

static const char *TAG = "board_bl";

esp_err_t board_backlight_init(void)
{
    const ledc_timer_config_t timer_cfg = {
        .speed_mode = BL_LEDC_MODE,
        .duty_resolution = BL_LEDC_RESOLUTION,
        .timer_num = BL_LEDC_TIMER,
        .freq_hz = BL_LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_cfg), TAG, "LEDC timer config failed");

    const ledc_channel_config_t channel_cfg = {
        .gpio_num = BOARD_LCD_BACKLIGHT,
        .speed_mode = BL_LEDC_MODE,
        .channel = BL_LEDC_CHANNEL,
        .timer_sel = BL_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel_cfg), TAG, "LEDC channel config failed");
    return ESP_OK;
}

esp_err_t board_backlight_set(uint8_t percent)
{
    const uint32_t clamped = (percent > BL_PERCENT_MAX) ? BL_PERCENT_MAX : percent;
    const uint32_t duty = (BL_DUTY_MAX * clamped) / BL_PERCENT_MAX;

    ESP_RETURN_ON_ERROR(ledc_set_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL, duty), TAG, "LEDC set duty failed");
    ESP_RETURN_ON_ERROR(ledc_update_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL), TAG, "LEDC update duty failed");
    return ESP_OK;
}
