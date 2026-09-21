#include "tokki_oled.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

esp_err_t tokki_oled_scroll_text(const char *text)
{
    int width = tokki_oled_marquee_width(text);
    if (width == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t framebuffer[TOKKI_OLED_FRAME_SIZE];
    for (int left = TOKKI_OLED_WIDTH; left > -width; left -= TOKKI_OLED_MARQUEE_STEP_PIXELS) {
        esp_err_t result = tokki_oled_render_marquee(framebuffer, sizeof(framebuffer), text, left);
        if (result == ESP_OK) {
            result = tokki_oled_draw_frame(framebuffer, sizeof(framebuffer));
        }
        if (result != ESP_OK) {
            return result;
        }
        vTaskDelay(pdMS_TO_TICKS(TOKKI_OLED_MARQUEE_FRAME_MS));
    }
    return ESP_OK;
}
