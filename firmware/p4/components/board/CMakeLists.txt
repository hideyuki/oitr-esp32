if(CONFIG_BOARD_PANEL_OLD)
    set(panel_init_src "panel_init_old.c")
else()
    set(panel_init_src "panel_init_new.c")
endif()

idf_component_register(SRCS "board_lcd.c"
                            "board_backlight.c"
                            "board_sdcard.c"
                            "${panel_init_src}"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_driver_gpio esp_lcd
                       PRIV_REQUIRES esp_driver_ledc esp_hw_support esp_lcd_jd9365
                                     esp_driver_sdmmc sdmmc fatfs)
