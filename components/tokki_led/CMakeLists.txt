idf_component_register(
    SRCS "tokki_led.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio tokki_board
)