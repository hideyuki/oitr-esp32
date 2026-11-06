#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Power the card from on-chip LDO channel 4, bring up SDMMC slot 0 in 4-bit
 * high-speed mode and mount its FAT filesystem at `mount_point` (e.g. "/sdcard").
 * The card is never formatted. Only one card can be mounted at a time.
 */
esp_err_t board_sdcard_mount(const char *mount_point);

/* Unmount the filesystem and release the LDO power control. */
esp_err_t board_sdcard_unmount(void);

#ifdef __cplusplus
}
#endif
