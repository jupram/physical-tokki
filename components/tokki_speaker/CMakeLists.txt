idf_component_register(
    SRCS "speaker_test.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_driver_i2s tokki_board
    EMBED_FILES "audio/hello_ram.wav"
)