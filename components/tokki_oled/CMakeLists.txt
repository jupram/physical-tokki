idf_component_register(
    SRCS "pet_eyes.c" "oled_art.c" "tokki_oled.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_driver_gpio esp_driver_i2c esp_lcd tokki_board
)