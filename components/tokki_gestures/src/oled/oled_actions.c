#include "oled/oled_actions.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pet_eyes.h"
#include "tokki_oled.h"

static esp_err_t animate_eyes(pet_eye_expression_t expression,
                              unsigned first_frame, unsigned frame_count)
{
    uint8_t framebuffer[TOKKI_OLED_FRAME_SIZE];
    for (unsigned frame = first_frame; frame < first_frame + frame_count; ++frame) {
        pet_eyes_render(framebuffer, sizeof(framebuffer),
                         TOKKI_OLED_WIDTH, TOKKI_OLED_HEIGHT, expression, frame);
        esp_err_t err = tokki_oled_draw_frame(framebuffer, sizeof(framebuffer));
        if (err != ESP_OK) {
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(60));
    }
    return ESP_OK;
}

static esp_err_t run_happy(void)
{
    return animate_eyes(PET_EYES_HAPPY, 0, 48);
}

static esp_err_t run_sad(void)
{
    return animate_eyes(PET_EYES_SAD, 0, 48);
}

static esp_err_t run_surprised(void)
{
    return animate_eyes(PET_EYES_SURPRISED, 0, 48);
}

static esp_err_t run_blink(void)
{
    return animate_eyes(PET_EYES_HAPPY, 34, 9);
}

static esp_err_t run_curious(void)
{
    return animate_eyes(PET_EYES_CURIOUS, 0, 48);
}

static esp_err_t animate_art(tokki_oled_art_t art, unsigned frame_count, unsigned frame_ms)
{
    uint8_t framebuffer[TOKKI_OLED_FRAME_SIZE];
    for (unsigned frame = 0; frame < frame_count; ++frame) {
        esp_err_t err = tokki_oled_render_art(framebuffer, sizeof(framebuffer), art, frame);
        if (err == ESP_OK) {
            err = tokki_oled_draw_frame(framebuffer, sizeof(framebuffer));
        }
        if (err != ESP_OK) {
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(frame_ms));
    }
    return ESP_OK;
}

static esp_err_t run_drink_water(void)
{
    return animate_art(TOKKI_OLED_ART_DRINK_WATER, 1, 2000);
}

static esp_err_t run_water_drop(void)
{
    return animate_art(TOKKI_OLED_ART_WATER_DROP, 24, 60);
}

static esp_err_t run_fire(void)
{
    return animate_art(TOKKI_OLED_ART_FIRE, 24, 60);
}

const tokki_action_descriptor_t TOKKI_OLED_HAPPY_ACTION = {
    .id = "oled.happy", .display_name = "Happy eyes",
    .device = TOKKI_DEVICE_OLED, .cancellable = false, .run = run_happy,
};
const tokki_action_descriptor_t TOKKI_OLED_SAD_ACTION = {
    .id = "oled.sad", .display_name = "Sad eyes",
    .device = TOKKI_DEVICE_OLED, .cancellable = false, .run = run_sad,
};
const tokki_action_descriptor_t TOKKI_OLED_SURPRISED_ACTION = {
    .id = "oled.surprised", .display_name = "Surprised eyes",
    .device = TOKKI_DEVICE_OLED, .cancellable = false, .run = run_surprised,
};
const tokki_action_descriptor_t TOKKI_OLED_BLINK_ACTION = {
    .id = "oled.blink", .display_name = "Blink eyes",
    .device = TOKKI_DEVICE_OLED, .cancellable = false, .run = run_blink,
};
const tokki_action_descriptor_t TOKKI_OLED_CURIOUS_ACTION = {
    .id = "oled.curious", .display_name = "Curious eyes",
    .device = TOKKI_DEVICE_OLED, .cancellable = false, .run = run_curious,
};
const tokki_action_descriptor_t TOKKI_OLED_DRINK_WATER_ACTION = {
    .id = "oled.drink_water", .display_name = "Drink water message",
    .device = TOKKI_DEVICE_OLED, .cancellable = false, .run = run_drink_water,
};
const tokki_action_descriptor_t TOKKI_OLED_WATER_DROP_ACTION = {
    .id = "oled.water_drop", .display_name = "Water drop",
    .device = TOKKI_DEVICE_OLED, .cancellable = false, .run = run_water_drop,
};
const tokki_action_descriptor_t TOKKI_OLED_FIRE_ACTION = {
    .id = "oled.fire", .display_name = "Fire",
    .device = TOKKI_DEVICE_OLED, .cancellable = false, .run = run_fire,
};