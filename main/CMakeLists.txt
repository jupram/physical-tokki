idf_component_register(
    SRCS "app_main.c"
    PRIV_REQUIRES
        tokki_board
        tokki_gestures
        tokki_led
        tokki_neopixel
        tokki_runtime
)
