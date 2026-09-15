#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pet_eyes.h"
#include "speaker_test.h"

#if CONFIG_FEATHER_TEST_ENABLE_WIFI_SCAN
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#endif

#define FEATHER_LED_GPIO GPIO_NUM_13
#define FEATHER_NEOPIXEL_GPIO GPIO_NUM_0
#define FEATHER_PERIPHERAL_POWER_GPIO GPIO_NUM_2
#define FEATHER_I2C_SDA_GPIO GPIO_NUM_22
#define FEATHER_I2C_SCL_GPIO GPIO_NUM_20

#define NEOPIXEL_RMT_RESOLUTION_HZ 10000000
#define NEOPIXEL_RAINBOW_BRIGHTNESS 32
#define NEOPIXEL_RAINBOW_STEPS 128
#define NEOPIXEL_RAINBOW_FRAME_MS 20
#define WIFI_SCAN_RESULT_LIMIT 12
#define SSD1306_I2C_ADDRESS 0x3C
#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 64
#define SSD1306_BUFFER_SIZE (SSD1306_WIDTH * SSD1306_HEIGHT / 8)
#define OLED_SCROLL_SCALE 3
#define OLED_SCROLL_STEP_PIXELS 2
#define OLED_SCROLL_FRAME_MS 45
#define OLED_SCROLL_TEXT "Hello Ram, Hiten, Shyam"
#define OLED_EYE_FRAME_MS 60
#define OLED_EYE_ANIMATION_FRAMES 48
#define OLED_COLOR_ZONE_DISPLAY_MS 900

static const char *TAG = "feather_test";

static rmt_channel_handle_t s_neopixel_channel;
static rmt_encoder_handle_t s_neopixel_encoder;
static esp_lcd_panel_handle_t s_oled_panel;
static uint8_t s_oled_framebuffer[SSD1306_BUFFER_SIZE];

typedef struct {
    char character;
    uint8_t columns[5];
} oled_glyph_t;

static const oled_glyph_t OLED_FONT[] = {
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00}},
    {',', {0x00, 0x50, 0x30, 0x00, 0x00}},
    {'H', {0x7F, 0x08, 0x08, 0x08, 0x7F}},
    {'R', {0x7F, 0x09, 0x19, 0x29, 0x46}},
    {'S', {0x46, 0x49, 0x49, 0x49, 0x31}},
    {'a', {0x20, 0x54, 0x54, 0x54, 0x78}},
    {'e', {0x38, 0x54, 0x54, 0x54, 0x18}},
    {'h', {0x7F, 0x08, 0x04, 0x04, 0x78}},
    {'i', {0x00, 0x44, 0x7D, 0x40, 0x00}},
    {'l', {0x00, 0x41, 0x7F, 0x40, 0x00}},
    {'m', {0x7C, 0x04, 0x18, 0x04, 0x78}},
    {'n', {0x7C, 0x08, 0x04, 0x04, 0x78}},
    {'o', {0x38, 0x44, 0x44, 0x44, 0x38}},
    {'r', {0x7C, 0x08, 0x04, 0x04, 0x08}},
    {'t', {0x04, 0x3F, 0x44, 0x40, 0x20}},
    {'y', {0x0C, 0x50, 0x50, 0x50, 0x3C}},
};

static esp_err_t configure_board_gpio(void)
{
    gpio_config_t output_config = {
        .pin_bit_mask = (1ULL << FEATHER_LED_GPIO) |
                        (1ULL << FEATHER_PERIPHERAL_POWER_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&output_config);
    if (err != ESP_OK) {
        return err;
    }

    err = gpio_set_level(FEATHER_LED_GPIO, 0);
    if (err != ESP_OK) {
        return err;
    }

    return gpio_set_level(FEATHER_PERIPHERAL_POWER_GPIO, 1);
}

static esp_err_t initialize_neopixel(void)
{
    rmt_tx_channel_config_t channel_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = FEATHER_NEOPIXEL_GPIO,
        .mem_block_symbols = 64,
        .resolution_hz = NEOPIXEL_RMT_RESOLUTION_HZ,
        .trans_queue_depth = 1,
    };

    esp_err_t err = rmt_new_tx_channel(&channel_config, &s_neopixel_channel);
    if (err != ESP_OK) {
        return err;
    }

    rmt_copy_encoder_config_t encoder_config = {};
    err = rmt_new_copy_encoder(&encoder_config, &s_neopixel_encoder);
    if (err != ESP_OK) {
        rmt_del_channel(s_neopixel_channel);
        s_neopixel_channel = NULL;
        return err;
    }

    err = rmt_enable(s_neopixel_channel);
    if (err != ESP_OK) {
        rmt_del_encoder(s_neopixel_encoder);
        rmt_del_channel(s_neopixel_channel);
        s_neopixel_encoder = NULL;
        s_neopixel_channel = NULL;
    }
    return err;
}

static esp_err_t set_neopixel(uint8_t red, uint8_t green, uint8_t blue)
{
    if (s_neopixel_channel == NULL || s_neopixel_encoder == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const uint8_t grb[] = {green, red, blue};
    rmt_symbol_word_t symbols[25] = {};
    size_t symbol_index = 0;

    for (size_t byte_index = 0; byte_index < sizeof(grb); ++byte_index) {
        for (int bit_index = 7; bit_index >= 0; --bit_index) {
            bool one = (grb[byte_index] & (1U << bit_index)) != 0;
            symbols[symbol_index++] = (rmt_symbol_word_t) {
                .level0 = 1,
                .duration0 = one ? 9 : 3,
                .level1 = 0,
                .duration1 = one ? 3 : 9,
            };
        }
    }

    symbols[symbol_index] = (rmt_symbol_word_t) {
        .level0 = 0,
        .duration0 = 250,
        .level1 = 0,
        .duration1 = 250,
    };

    rmt_transmit_config_t transmit_config = {
        .loop_count = 0,
    };
    esp_err_t err = rmt_transmit(
        s_neopixel_channel,
        s_neopixel_encoder,
        symbols,
        sizeof(symbols),
        &transmit_config
    );
    if (err != ESP_OK) {
        return err;
    }

    return rmt_tx_wait_all_done(s_neopixel_channel, pdMS_TO_TICKS(100));
}

static void neopixel_rainbow_color(uint8_t position,
                                   uint8_t *red,
                                   uint8_t *green,
                                   uint8_t *blue)
{
    uint16_t raw_red;
    uint16_t raw_green;
    uint16_t raw_blue;

    if (position < 85) {
        raw_red = 255 - position * 3;
        raw_green = position * 3;
        raw_blue = 0;
    } else if (position < 170) {
        position -= 85;
        raw_red = 0;
        raw_green = 255 - position * 3;
        raw_blue = position * 3;
    } else {
        position -= 170;
        raw_red = position * 3;
        raw_green = 0;
        raw_blue = 255 - position * 3;
    }

    *red = raw_red * NEOPIXEL_RAINBOW_BRIGHTNESS / 255;
    *green = raw_green * NEOPIXEL_RAINBOW_BRIGHTNESS / 255;
    *blue = raw_blue * NEOPIXEL_RAINBOW_BRIGHTNESS / 255;
}

static esp_err_t run_neopixel_rainbow(void)
{
    ESP_LOGI(TAG, "NeoPixel should smoothly cycle through the rainbow five times");
    for (int cycle = 0; cycle < 5; ++cycle) {
        for (int step = 0; step < NEOPIXEL_RAINBOW_STEPS; ++step) {
            uint8_t red;
            uint8_t green;
            uint8_t blue;
            uint8_t position = step * 256 / NEOPIXEL_RAINBOW_STEPS;
            neopixel_rainbow_color(position, &red, &green, &blue);

            esp_err_t err = set_neopixel(red, green, blue);
            if (err != ESP_OK) {
                return err;
            }
            vTaskDelay(pdMS_TO_TICKS(NEOPIXEL_RAINBOW_FRAME_MS));
        }
    }
    return ESP_OK;
}

static esp_err_t run_visual_test(void)
{
    ESP_LOGI(TAG, "Red status LED should blink three times");
    for (int i = 0; i < 3; ++i) {
        esp_err_t err = gpio_set_level(FEATHER_LED_GPIO, 1);
        if (err != ESP_OK) {
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(200));

        err = gpio_set_level(FEATHER_LED_GPIO, 0);
        if (err != ESP_OK) {
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    const uint8_t colors[][3] = {
        {24, 0, 0},
        {0, 24, 0},
        {0, 0, 24},
    };
    ESP_LOGI(TAG, "NeoPixel should cycle red, green, and blue five times");
    for (int cycle = 0; cycle < 5; ++cycle) {
        for (size_t i = 0; i < sizeof(colors) / sizeof(colors[0]); ++i) {
            esp_err_t err = set_neopixel(colors[i][0], colors[i][1], colors[i][2]);
            if (err != ESP_OK) {
                return err;
            }
            vTaskDelay(pdMS_TO_TICKS(350));
        }
    }

    esp_err_t rainbow_result = run_neopixel_rainbow();
    if (rainbow_result != ESP_OK) {
        return rainbow_result;
    }

    return set_neopixel(0, 0, 0);
}

#if CONFIG_FEATHER_TEST_ENABLE_WIFI_SCAN
static esp_err_t initialize_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        err = nvs_flash_erase();
        if (err != ESP_OK) {
            return err;
        }
        err = nvs_flash_init();
    }
    return err;
}

static esp_err_t run_wifi_scan(void)
{
    esp_err_t err = initialize_nvs();
    if (err != ESP_OK) {
        return err;
    }

    err = esp_netif_init();
    if (err != ESP_OK) {
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK) {
        return err;
    }

    esp_netif_t *station = esp_netif_create_default_wifi_sta();
    if (station == NULL) {
        return ESP_ERR_NO_MEM;
    }

    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wifi_config);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_wifi_start();
    if (err != ESP_OK) {
        return err;
    }

    err = esp_wifi_scan_start(NULL, true);
    if (err != ESP_OK) {
        return err;
    }

    uint16_t total_access_points = 0;
    err = esp_wifi_scan_get_ap_num(&total_access_points);
    if (err != ESP_OK) {
        return err;
    }

    wifi_ap_record_t records[WIFI_SCAN_RESULT_LIMIT] = {};
    uint16_t record_count = WIFI_SCAN_RESULT_LIMIT;
    err = esp_wifi_scan_get_ap_records(&record_count, records);
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "Wi-Fi radio found %u access point(s); showing %u",
             total_access_points, record_count);
    for (uint16_t i = 0; i < record_count; ++i) {
        ESP_LOGI(TAG, "  %-32.32s RSSI %4d dBm channel %u",
                 (const char *) records[i].ssid,
                 records[i].rssi,
                 records[i].primary);
    }

    return ESP_OK;
}
#endif

static esp_err_t initialize_i2c_bus(i2c_master_bus_handle_t *bus)
{
    if (bus == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = -1,
        .scl_io_num = FEATHER_I2C_SCL_GPIO,
        .sda_io_num = FEATHER_I2C_SDA_GPIO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    return i2c_new_master_bus(&bus_config, bus);
}

static esp_err_t run_i2c_scan(i2c_master_bus_handle_t bus,
                              uint8_t *device_count,
                              bool *oled_found)
{
    if (bus == NULL || device_count == NULL || oled_found == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *device_count = 0;
    *oled_found = false;

    ESP_LOGI(TAG, "Scanning STEMMA QT I2C bus (SDA GPIO22, SCL GPIO20)");
    for (uint8_t address = 0x08; address <= 0x77; ++address) {
        esp_err_t err = i2c_master_probe(bus, address, 20);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "  Found I2C device at 0x%02X", address);
            ++(*device_count);
            if (address == SSD1306_I2C_ADDRESS) {
                *oled_found = true;
            }
        } else if (err != ESP_ERR_NOT_FOUND && err != ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "  Probe 0x%02X returned %s", address, esp_err_to_name(err));
        }
    }

    if (*device_count == 0) {
        ESP_LOGW(TAG, "No I2C devices found");
    }
    return ESP_OK;
}

static void oled_set_pixel(int x, int y)
{
    if (x >= 0 && x < SSD1306_WIDTH && y >= 0 && y < SSD1306_HEIGHT) {
        s_oled_framebuffer[SSD1306_WIDTH * (y / 8) + x] |= 1U << (y % 8);
    }
}

static void oled_draw_color_zone(bool top_zone)
{
    memset(s_oled_framebuffer, 0, sizeof(s_oled_framebuffer));
    int start_y = top_zone ? 0 : 16;
    int end_y = top_zone ? 16 : SSD1306_HEIGHT;

    for (int y = start_y; y < end_y; ++y) {
        for (int x = 0; x < SSD1306_WIDTH; ++x) {
            oled_set_pixel(x, y);
        }
    }
}

static const uint8_t *oled_find_glyph(char character)
{
    for (size_t i = 0; i < sizeof(OLED_FONT) / sizeof(OLED_FONT[0]); ++i) {
        if (OLED_FONT[i].character == character) {
            return OLED_FONT[i].columns;
        }
    }
    return OLED_FONT[0].columns;
}

static void oled_draw_character(int x, int y, char character, int scale)
{
    const uint8_t *glyph = oled_find_glyph(character);
    for (int column = 0; column < 5; ++column) {
        for (int row = 0; row < 7; ++row) {
            if ((glyph[column] & (1U << row)) == 0) {
                continue;
            }
            for (int offset_y = 0; offset_y < scale; ++offset_y) {
                for (int offset_x = 0; offset_x < scale; ++offset_x) {
                    oled_set_pixel(
                        x + column * scale + offset_x,
                        y + row * scale + offset_y
                    );
                }
            }
        }
    }
}

static int oled_text_width(const char *text, int scale)
{
    size_t length = strlen(text);
    return (int) length * 6 * scale - scale;
}

static void oled_draw_text(int x, int y, const char *text, int scale)
{
    int character_width = 6 * scale;
    size_t length = strlen(text);
    for (size_t i = 0; i < length; ++i) {
        oled_draw_character(x + (int) i * character_width,
                            y,
                            text[i],
                            scale);
    }
}

static esp_err_t oled_push_frame(void)
{
    return esp_lcd_panel_draw_bitmap(
        s_oled_panel,
        0,
        0,
        SSD1306_WIDTH,
        SSD1306_HEIGHT,
        s_oled_framebuffer
    );
}

static esp_err_t oled_animate_eyes(pet_eye_expression_t expression,
                                   const char *name)
{
    for (unsigned int frame = 0;
         frame < OLED_EYE_ANIMATION_FRAMES;
         ++frame) {
        pet_eyes_render(s_oled_framebuffer,
                        sizeof(s_oled_framebuffer),
                        SSD1306_WIDTH,
                        SSD1306_HEIGHT,
                        expression,
                        frame);
        esp_err_t err = oled_push_frame();
        if (err != ESP_OK) {
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(OLED_EYE_FRAME_MS));
    }

    ESP_LOGI(TAG, "OLED animated %s eyes", name);
    return ESP_OK;
}

static void oled_scroll_task(void *arg)
{
    int text_width = oled_text_width(OLED_SCROLL_TEXT, OLED_SCROLL_SCALE);
    int text_height = 7 * OLED_SCROLL_SCALE;
    int y = (SSD1306_HEIGHT - text_height) / 2;

    while (true) {
        oled_draw_color_zone(true);
        esp_err_t err = oled_push_frame();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "OLED top color-zone test failed: %s",
                     esp_err_to_name(err));
            vTaskDelete(NULL);
        }
        ESP_LOGI(TAG, "OLED color test: top 16 rows illuminated");
        vTaskDelay(pdMS_TO_TICKS(OLED_COLOR_ZONE_DISPLAY_MS));

        oled_draw_color_zone(false);
        err = oled_push_frame();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "OLED lower color-zone test failed: %s",
                     esp_err_to_name(err));
            vTaskDelete(NULL);
        }
        ESP_LOGI(TAG, "OLED color test: lower 48 rows illuminated");
        vTaskDelay(pdMS_TO_TICKS(OLED_COLOR_ZONE_DISPLAY_MS));

        const pet_eye_expression_t expressions[] = {
            PET_EYES_HAPPY,
            PET_EYES_SAD,
            PET_EYES_CURIOUS,
        };
        const char *const expression_names[] = {
            "happy",
            "sad",
            "curious",
        };

        for (size_t i = 0;
             i < sizeof(expressions) / sizeof(expressions[0]);
             ++i) {
            err = oled_animate_eyes(expressions[i], expression_names[i]);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "OLED %s eye animation failed: %s",
                         expression_names[i],
                         esp_err_to_name(err));
                vTaskDelete(NULL);
            }
        }

        for (int x = SSD1306_WIDTH;
             x > -text_width;
             x -= OLED_SCROLL_STEP_PIXELS) {
            memset(s_oled_framebuffer, 0, sizeof(s_oled_framebuffer));
            oled_draw_text(x,
                           y,
                           OLED_SCROLL_TEXT,
                           OLED_SCROLL_SCALE);

            esp_err_t err = oled_push_frame();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "OLED scroll failed: %s", esp_err_to_name(err));
                vTaskDelete(NULL);
            }
            vTaskDelay(pdMS_TO_TICKS(OLED_SCROLL_FRAME_MS));
        }
    }
}

static esp_err_t run_ssd1306_test(i2c_master_bus_handle_t bus)
{
    if (bus == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = i2c_master_probe(bus, SSD1306_I2C_ADDRESS, 100);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SSD1306 did not respond at I2C address 0x%02X",
                 SSD1306_I2C_ADDRESS);
        return err;
    }

    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = SSD1306_I2C_ADDRESS,
        .scl_speed_hz = 400000,
        .transaction_timeout_ms = 1000,
        .control_phase_bytes = 1,
        .dc_bit_offset = 6,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };

    esp_lcd_panel_io_handle_t io = NULL;
    err = esp_lcd_new_panel_io_i2c(bus, &io_config, &io);
    if (err != ESP_OK) {
        return err;
    }

    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = SSD1306_HEIGHT,
        .contrast = 160,
    };
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = GPIO_NUM_NC,
        .bits_per_pixel = 1,
        .vendor_config = &ssd1306_config,
    };

    err = esp_lcd_new_panel_ssd1306(io, &panel_config, &s_oled_panel);
    if (err != ESP_OK) {
        esp_lcd_panel_io_del(io);
        return err;
    }

    err = esp_lcd_panel_reset(s_oled_panel);
    if (err == ESP_OK) {
        err = esp_lcd_panel_init(s_oled_panel);
    }
    if (err == ESP_OK) {
        err = esp_lcd_panel_disp_on_off(s_oled_panel, true);
    }
    if (err == ESP_OK) {
        BaseType_t task_result = xTaskCreate(
            oled_scroll_task,
            "oled_scroll",
            3072,
            NULL,
            5,
            NULL
        );
        if (task_result != pdPASS) {
            err = ESP_ERR_NO_MEM;
        } else {
            ESP_LOGI(TAG, "OLED animating eyes and scrolling: %s",
                     OLED_SCROLL_TEXT);
        }
    }

    if (err != ESP_OK) {
        esp_lcd_panel_del(s_oled_panel);
        esp_lcd_panel_io_del(io);
        s_oled_panel = NULL;
    }
    return err;
}

static void print_system_information(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    uint32_t flash_size = 0;
    esp_err_t flash_err = esp_flash_get_size(NULL, &flash_size);

    uint8_t mac[6] = {};
    esp_err_t mac_err = esp_read_mac(mac, ESP_MAC_WIFI_STA);

    ESP_LOGI(TAG, "Adafruit ESP32 Feather V2 self-test");
    ESP_LOGI(TAG, "Chip: %d core(s), revision %u, features: Wi-Fi%s%s",
             chip_info.cores,
             chip_info.revision,
             (chip_info.features & CHIP_FEATURE_BT) ? ", BT" : "",
             (chip_info.features & CHIP_FEATURE_BLE) ? ", BLE" : "");
    if (flash_err == ESP_OK) {
        ESP_LOGI(TAG, "Flash: %" PRIu32 " MB", flash_size / (1024 * 1024));
    } else {
        ESP_LOGE(TAG, "Flash size read failed: %s", esp_err_to_name(flash_err));
    }
    ESP_LOGI(TAG, "Free heap: %" PRIu32 " bytes", esp_get_free_heap_size());
    if (mac_err == ESP_OK) {
        ESP_LOGI(TAG, "Wi-Fi MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        ESP_LOGE(TAG, "MAC address read failed: %s", esp_err_to_name(mac_err));
    }
}

static bool report_test_result(const char *name, esp_err_t result)
{
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "[PASS] %s", name);
        return true;
    }

    ESP_LOGE(TAG, "[FAIL] %s: %s", name, esp_err_to_name(result));
    return false;
}

void app_main(void)
{
    print_system_information();

    bool all_passed = true;
    all_passed &= report_test_result("Board GPIO and peripheral power",
                                     configure_board_gpio());

    esp_err_t neopixel_init_result = initialize_neopixel();
    all_passed &= report_test_result("NeoPixel RMT initialization",
                                     neopixel_init_result);
    if (neopixel_init_result == ESP_OK) {
        all_passed &= report_test_result("Visible LED sequence",
                                         run_visual_test());
    }

#if CONFIG_FEATHER_TEST_ENABLE_WIFI_SCAN
    all_passed &= report_test_result("Wi-Fi scan", run_wifi_scan());
#else
    ESP_LOGI(TAG, "[SKIP] Wi-Fi scan disabled in project configuration");
#endif

#if CONFIG_FEATHER_TEST_ENABLE_SPEAKER_TEST
    ESP_LOGI(TAG,
             "Starting MAX98357A speaker test at a hard maximum of 20%%");
    all_passed &= report_test_result("MAX98357A speaker test",
                                     speaker_test_run());
#else
    ESP_LOGI(TAG, "[SKIP] MAX98357A speaker test disabled");
#endif

    i2c_master_bus_handle_t i2c_bus = NULL;
    esp_err_t i2c_init_result = initialize_i2c_bus(&i2c_bus);
    all_passed &= report_test_result("STEMMA QT I2C initialization",
                                     i2c_init_result);
    if (i2c_init_result == ESP_OK) {
        uint8_t i2c_device_count = 0;
        bool oled_found = false;
        all_passed &= report_test_result(
            "STEMMA QT I2C scan",
            run_i2c_scan(i2c_bus, &i2c_device_count, &oled_found)
        );
        if (!oled_found) {
            ESP_LOGI(TAG,
                     "[SKIP] SSD1306 OLED visual test: no display at address 0x%02X",
                     SSD1306_I2C_ADDRESS);
        } else {
            all_passed &= report_test_result("SSD1306 OLED visual test",
                                             run_ssd1306_test(i2c_bus));
        }
    }

    if (all_passed) {
        ESP_LOGI(TAG, "SELF-TEST PASSED");
        if (set_neopixel(0, 16, 0) != ESP_OK) {
            ESP_LOGW(TAG, "Could not set final green NeoPixel status");
        }
    } else {
        ESP_LOGE(TAG, "SELF-TEST FAILED; review the failures above");
        if (set_neopixel(16, 0, 0) != ESP_OK) {
            ESP_LOGW(TAG, "Could not set final red NeoPixel status");
        }
    }

    while (true) {
        gpio_set_level(FEATHER_LED_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(FEATHER_LED_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(1900));
    }
}
