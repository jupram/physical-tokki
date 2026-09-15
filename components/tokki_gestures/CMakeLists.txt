idf_component_register(
    SRCS
        "tokki_gestures.c"
        "src/led/led_actions.c"
        "src/neopixel/neopixel_actions.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES tokki_led tokki_neopixel
)