set(private_requirements
    esp_driver_gpio
    esp_driver_i2c
    esp_driver_i2s
    esp_driver_rmt
    esp_lcd
    esp_system
    spi_flash
)

if(CONFIG_FEATHER_TEST_ENABLE_WIFI_SCAN)
    list(APPEND private_requirements
        esp_event
        esp_netif
        esp_wifi
        nvs_flash
    )
endif()

idf_component_register(
    SRCS "feather_v2_self_test.c" "pet_eyes.c" "speaker_test.c"
    PRIV_REQUIRES ${private_requirements}
    EMBED_FILES "audio/hello_ram.wav"
)
