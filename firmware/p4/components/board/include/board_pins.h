#pragma once

#include "driver/gpio.h"

/* LCD (JD9365, MIPI-DSI 2-lane) */
#define BOARD_LCD_RST            GPIO_NUM_27
#define BOARD_LCD_BACKLIGHT      GPIO_NUM_23

/* Touch (GSL3680) */
#define BOARD_TOUCH_RST          GPIO_NUM_22
#define BOARD_TOUCH_INT          GPIO_NUM_21

/* Shared I2C bus: touch, codec, camera SCCB */
#define BOARD_I2C_SDA            GPIO_NUM_7
#define BOARD_I2C_SCL            GPIO_NUM_8

/* SD card (SDMMC 4-bit) */
#define BOARD_SD_D0              GPIO_NUM_39
#define BOARD_SD_D1              GPIO_NUM_40
#define BOARD_SD_D2              GPIO_NUM_41
#define BOARD_SD_D3              GPIO_NUM_42
#define BOARD_SD_CMD             GPIO_NUM_44
#define BOARD_SD_CLK             GPIO_NUM_43
#define BOARD_SD_LDO_CHAN        4   /* on-chip LDO_VO4 powers the card */

/* Audio (ES8311 over I2S) */
#define BOARD_I2S_MCLK           GPIO_NUM_13
#define BOARD_I2S_BCLK           GPIO_NUM_12
#define BOARD_I2S_WS             GPIO_NUM_10
#define BOARD_I2S_DOUT           GPIO_NUM_9
#define BOARD_I2S_DIN            GPIO_NUM_11
#define BOARD_AUDIO_PA           GPIO_NUM_20

/* MIPI DSI PHY power (required) */
#define BOARD_MIPI_DSI_LDO_CHAN  3
#define BOARD_MIPI_DSI_LDO_MV    2500

/* Panel native resolution (portrait) */
#define BOARD_LCD_H_RES          800
#define BOARD_LCD_V_RES          1280

/* Logical landscape layout: video on top, status bar below */
#define BOARD_DISPLAY_W          1280
#define BOARD_DISPLAY_H          800
#define BOARD_VIDEO_H            720
#define BOARD_STATUS_BAR_H       80

/*
 * Logical landscape -> portrait frame buffer mapping (verified on hardware):
 * logical (x right, y down; 0..1279 x 0..799) -> frame buffer (col = 799 - y, row = x).
 * Frame-buffer column 799 is the logical top edge; the status bar (logical
 * y = 720..799) occupies the inclusive column range below. Video files are
 * pre-rotated into this layout by tools/convert_video.sh.
 */
#define BOARD_STATUS_BAR_FB_X0   0
#define BOARD_STATUS_BAR_FB_X1   (BOARD_STATUS_BAR_H - 1)
