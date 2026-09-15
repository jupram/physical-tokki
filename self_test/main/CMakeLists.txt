set(private_requirements
    esp_driver_gpio
    esp_driver_i2c
    esp_lcd
    esp_system
    spi_flash
    tokki_board
    tokki_led
    tokki_neopixel
    tokki_oled
    tokki_speaker
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
    SRCS "self_test_main.c"
    PRIV_REQUIRES ${private_requirements}
)