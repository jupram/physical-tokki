idf_component_register(
    SRCS "tokki_protocol.c" "tokki_idle.c" "tokki_runtime.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_driver_uart tokki_gestures tokki_oled tokki_led
)
