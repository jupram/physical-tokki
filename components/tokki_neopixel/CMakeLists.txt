idf_component_register(
    SRCS "tokki_neopixel.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_rmt tokki_board
)