#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"

#include "board_pins.h"

static const char *TAG = "oitr";

void app_main(void)
{
    ESP_LOGI(TAG, "ESP-IDF %s", esp_get_idf_version());
    ESP_LOGI(TAG, "Free internal heap: %u bytes",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGI(TAG, "Free PSRAM: %u bytes",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    ESP_LOGI(TAG, "LCD: RST=%d BL=%d native=%dx%d logical=%dx%d",
             BOARD_LCD_RST, BOARD_LCD_BACKLIGHT,
             BOARD_LCD_H_RES, BOARD_LCD_V_RES,
             BOARD_DISPLAY_W, BOARD_DISPLAY_H);
    ESP_LOGI(TAG, "SD: CLK=%d CMD=%d D0=%d D1=%d D2=%d D3=%d",
             BOARD_SD_CLK, BOARD_SD_CMD,
             BOARD_SD_D0, BOARD_SD_D1, BOARD_SD_D2, BOARD_SD_D3);
}
