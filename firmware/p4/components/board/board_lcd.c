#include "esp_check.h"
#include "esp_lcd_jd9365.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"

#include "board_lcd.h"
#include "board_pins.h"
#include "panel_init.h"

#define LCD_DSI_BUS_ID        0
#define LCD_DSI_LANES         2
#define LCD_VIRTUAL_CHANNEL   0
#define LCD_CMD_BITS          8
#define LCD_PARAM_BITS        8
#define LCD_NUM_FBS           2
#define LCD_BITS_PER_PIXEL    16

static const char *TAG = "board_lcd";

static esp_err_t create_dsi_io(esp_lcd_dsi_bus_handle_t *out_bus, esp_lcd_panel_io_handle_t *out_io)
{
    const esp_lcd_dsi_bus_config_t bus_cfg = {
        .bus_id = LCD_DSI_BUS_ID,
        .num_data_lanes = LCD_DSI_LANES,
        /* 0: driver picks PLL_F20M when built for rev < 3 (our v1.3), XTAL otherwise. */
        .phy_clk_src = 0,
        .lane_bit_rate_mbps = (float)board_panel_timing.lane_bit_rate_mbps,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_dsi_bus(&bus_cfg, out_bus), TAG, "new DSI bus failed");

    const esp_lcd_dbi_io_config_t dbi_cfg = {
        .virtual_channel = LCD_VIRTUAL_CHANNEL,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
    };
    const esp_err_t err = esp_lcd_new_panel_io_dbi(*out_bus, &dbi_cfg, out_io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "new DBI panel IO failed: %s", esp_err_to_name(err));
        esp_lcd_del_dsi_bus(*out_bus);
        *out_bus = NULL;
    }
    return err;
}

static esp_err_t create_panel(esp_lcd_dsi_bus_handle_t bus, esp_lcd_panel_io_handle_t io,
                              esp_lcd_panel_handle_t *out_panel)
{
    const board_panel_timing_t *t = &board_panel_timing;
    const esp_lcd_dpi_panel_config_t dpi_cfg = {
        .virtual_channel = LCD_VIRTUAL_CHANNEL,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = (float)t->dpi_clock_freq_mhz,
        .in_color_format = LCD_COLOR_FMT_RGB565,
        .out_color_format = LCD_COLOR_FMT_RGB565,
        .num_fbs = LCD_NUM_FBS,
        .video_timing = {
            .h_size = BOARD_LCD_H_RES,
            .v_size = BOARD_LCD_V_RES,
            .hsync_pulse_width = t->hsync_pulse_width,
            .hsync_back_porch = t->hsync_back_porch,
            .hsync_front_porch = t->hsync_front_porch,
            .vsync_pulse_width = t->vsync_pulse_width,
            .vsync_back_porch = t->vsync_back_porch,
            .vsync_front_porch = t->vsync_front_porch,
        },
    };
    const jd9365_vendor_config_t vendor_cfg = {
        .init_cmds = board_panel_init_cmds,
        .init_cmds_size = (uint16_t)board_panel_init_cmds_len,
        .mipi_config = {
            .dsi_bus = bus,
            .dpi_config = &dpi_cfg,
            .lane_num = LCD_DSI_LANES,
        },
    };
    const esp_lcd_panel_dev_config_t dev_cfg = {
        .reset_gpio_num = BOARD_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BITS_PER_PIXEL,
        .vendor_config = (void *)&vendor_cfg,
    };
    /* The jd9365 driver copies what it needs from the configs during creation. */
    return esp_lcd_new_panel_jd9365(io, &dev_cfg, out_panel);
}

static esp_err_t start_panel(esp_lcd_panel_handle_t panel)
{
    ESP_RETURN_ON_ERROR(esp_lcd_dpi_panel_enable_dma2d(panel), TAG, "enable DMA2D failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel), TAG, "panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), TAG, "panel init failed");
    return ESP_OK;
}

esp_err_t board_lcd_init(esp_lcd_panel_handle_t *out_panel)
{
    ESP_RETURN_ON_FALSE(out_panel != NULL, ESP_ERR_INVALID_ARG, TAG, "out_panel is NULL");

    esp_ldo_channel_handle_t ldo = NULL;
    esp_lcd_dsi_bus_handle_t bus = NULL;
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_handle_t panel = NULL;
    esp_err_t ret = ESP_OK;

    const esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = BOARD_MIPI_DSI_LDO_CHAN,
        .voltage_mv = BOARD_MIPI_DSI_LDO_MV,
    };
    ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&ldo_cfg, &ldo), TAG, "acquire DSI PHY LDO failed");
    ESP_GOTO_ON_ERROR(create_dsi_io(&bus, &io), err, TAG, "DSI bus/IO setup failed");
    ESP_GOTO_ON_ERROR(create_panel(bus, io, &panel), err, TAG, "new JD9365 panel failed");
    ESP_GOTO_ON_ERROR(start_panel(panel), err, TAG, "panel start failed");

    ESP_LOGI(TAG, "JD9365 up: %dx%d, DPI %u MHz, %u Mbps x %d lanes, %u init cmds",
             BOARD_LCD_H_RES, BOARD_LCD_V_RES,
             (unsigned)board_panel_timing.dpi_clock_freq_mhz,
             (unsigned)board_panel_timing.lane_bit_rate_mbps, LCD_DSI_LANES,
             (unsigned)board_panel_init_cmds_len);
    *out_panel = panel;
    return ESP_OK;

err:
    if (panel != NULL) {
        esp_lcd_panel_del(panel);
    }
    if (io != NULL) {
        esp_lcd_panel_io_del(io);
    }
    if (bus != NULL) {
        esp_lcd_del_dsi_bus(bus);
    }
    esp_ldo_release_channel(ldo);
    return ret;
}

esp_err_t board_lcd_get_frame_buffers(esp_lcd_panel_handle_t panel, void **fb0, void **fb1)
{
    ESP_RETURN_ON_FALSE(panel != NULL && fb0 != NULL && fb1 != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "invalid argument");
    return esp_lcd_dpi_panel_get_frame_buffer(panel, LCD_NUM_FBS, fb0, fb1);
}
