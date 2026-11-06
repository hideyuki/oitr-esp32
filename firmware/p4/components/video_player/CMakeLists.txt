idf_component_register(SRCS "video_player.c"
                            "video_decode.c"
                            "video_present.c"
                            "video_stats.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_lcd
                       PRIV_REQUIRES esp_driver_jpeg esp_timer freertos
                                     mjpeg_reader board)
