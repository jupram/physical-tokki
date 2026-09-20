idf_component_register(
    SRCS "tokki_protocol.c" "tokki_idle.c" "tokki_runtime.c"
    INCLUDE_DIRS "include"
    REQUIRES tokki_gestures
    PRIV_REQUIRES esp_driver_uart esp_hw_support tokki_oled tokki_led tokki_neopixel
)
