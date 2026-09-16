idf_component_register(
    SRCS "tokki_neopixel.c" "neopixel_effects.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_rmt tokki_board
)