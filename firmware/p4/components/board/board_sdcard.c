#include <stdio.h>
#include <string.h>

#include "driver/sdmmc_host.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "sdmmc_cmd.h"

#include "board_pins.h"
#include "board_sdcard.h"

#define SD_BUS_WIDTH           4
#define SD_MAX_OPEN_FILES      4
#define SD_ALLOC_UNIT_SIZE     (16 * 1024)
#define SD_MOUNT_POINT_MAX     16

static const char *TAG = "board_sd";

static sdmmc_card_t *s_card;
static sd_pwr_ctrl_handle_t s_pwr;
static char s_mount_point[SD_MOUNT_POINT_MAX];

static sdmmc_slot_config_t make_slot_config(void)
{
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.clk = BOARD_SD_CLK;
    slot.cmd = BOARD_SD_CMD;
    slot.d0 = BOARD_SD_D0;
    slot.d1 = BOARD_SD_D1;
    slot.d2 = BOARD_SD_D2;
    slot.d3 = BOARD_SD_D3;
    slot.width = SD_BUS_WIDTH;
    slot.flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    return slot;
}

static void release_power(void)
{
    if (s_pwr != NULL) {
        esp_err_t err = sd_pwr_ctrl_del_on_chip_ldo(s_pwr);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "release SD LDO failed: %s", esp_err_to_name(err));
        }
        s_pwr = NULL;
    }
}

esp_err_t board_sdcard_mount(const char *mount_point)
{
    ESP_RETURN_ON_FALSE(mount_point != NULL && strlen(mount_point) < SD_MOUNT_POINT_MAX,
                        ESP_ERR_INVALID_ARG, TAG, "invalid mount point");
    ESP_RETURN_ON_FALSE(s_card == NULL, ESP_ERR_INVALID_STATE, TAG, "already mounted");

    const sd_pwr_ctrl_ldo_config_t ldo_cfg = { .ldo_chan_id = BOARD_SD_LDO_CHAN };
    ESP_RETURN_ON_ERROR(sd_pwr_ctrl_new_on_chip_ldo(&ldo_cfg, &s_pwr), TAG, "SD LDO power control failed");

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
    host.pwr_ctrl_handle = s_pwr;

    const sdmmc_slot_config_t slot = make_slot_config();
    const esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = SD_MAX_OPEN_FILES,
        .allocation_unit_size = SD_ALLOC_UNIT_SIZE,
    };

    esp_err_t err = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot, &mount_cfg, &s_card);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mount %s failed: %s", mount_point, esp_err_to_name(err));
        s_card = NULL;
        release_power();
        return err;
    }
    snprintf(s_mount_point, sizeof(s_mount_point), "%s", mount_point);
    ESP_LOGI(TAG, "mounted at %s", s_mount_point);
    sdmmc_card_print_info(stdout, s_card);
    return ESP_OK;
}

esp_err_t board_sdcard_unmount(void)
{
    ESP_RETURN_ON_FALSE(s_card != NULL, ESP_ERR_INVALID_STATE, TAG, "not mounted");

    /* Unmount first: the card must stay powered while the host is torn down. */
    esp_err_t err = esp_vfs_fat_sdcard_unmount(s_mount_point, s_card);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "unmount failed: %s", esp_err_to_name(err));
        return err;
    }
    s_card = NULL;
    s_mount_point[0] = '\0';
    release_power();
    return ESP_OK;
}
