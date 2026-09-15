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
        bool restore = expression >= PET_EYES_WINK && frame == first_frame + frame_count - 1;
        pet_eyes_render(framebuffer, sizeof(framebuffer),
                 TOKKI_OLED_WIDTH, TOKKI_OLED_HEIGHT,
                 restore ? PET_EYES_HAPPY : expression, restore ? 0 : frame);
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

static esp_err_t run_wink(void)
{
    return animate_eyes(PET_EYES_WINK, 34, 10);
}

static esp_err_t run_look_left(void)
{
    return animate_eyes(PET_EYES_LOOK_LEFT, 0, 24);
}

static esp_err_t run_look_right(void)
{
    return animate_eyes(PET_EYES_LOOK_RIGHT, 0, 24);
}

static esp_err_t run_look_up(void)
{
    return animate_eyes(PET_EYES_LOOK_UP, 0, 24);
}

static esp_err_t run_look_down(void)
{
    return animate_eyes(PET_EYES_LOOK_DOWN, 0, 24);
}

static esp_err_t run_sleepy(void)
{
    return animate_eyes(PET_EYES_SLEEPY, 0, 24);
}

static esp_err_t animate_art(tokki_oled_art_t art, unsigned frame_count, unsigned frame_ms)
{
    uint8_t framebuffer[TOKKI_OLED_FRAME_SIZE];
    for (unsigned frame = 0; frame < frame_count; ++frame) {
        esp_err_t err = ESP_OK;
        if (art >= TOKKI_OLED_ART_CHECKMARK && frame == frame_count - 1) {
            pet_eyes_render(framebuffer, sizeof(framebuffer), TOKKI_OLED_WIDTH,
                             TOKKI_OLED_HEIGHT, PET_EYES_HAPPY, 0);
        } else {
            err = tokki_oled_render_art(framebuffer, sizeof(framebuffer), art, frame);
        }
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

static esp_err_t run_checkmark(void)
{
    return animate_art(TOKKI_OLED_ART_CHECKMARK, 17, 60);
}

static esp_err_t run_thinking(void)
{
    return animate_art(TOKKI_OLED_ART_THINKING, 20, 60);
}

static esp_err_t run_heart(void)
{
    return animate_art(TOKKI_OLED_ART_HEART, 24, 60);
}

static esp_err_t run_exclamation(void)
{
    return animate_art(TOKKI_OLED_ART_EXCLAMATION, 17, 60);
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

const tokki_action_descriptor_t TOKKI_OLED_WINK_ACTION = {
    .id = "oled.wink", .display_name = "Wink",
    .device = TOKKI_DEVICE_OLED, .cancellable = false, .run = run_wink,
};

const tokki_action_descriptor_t TOKKI_OLED_CHECKMARK_ACTION = {
    .id = "oled.checkmark", .display_name = "Checkmark",
    .device = TOKKI_DEVICE_OLED, .cancellable = false, .run = run_checkmark,
};
const tokki_action_descriptor_t TOKKI_OLED_THINKING_ACTION = {
    .id = "oled.thinking", .display_name = "Thinking dots",
    .device = TOKKI_DEVICE_OLED, .cancellable = false, .run = run_thinking,
};

const tokki_action_descriptor_t TOKKI_OLED_LOOK_LEFT_ACTION = {
    .id = "oled.look_left", .display_name = "Look left",
    .device = TOKKI_DEVICE_OLED, .cancellable = false, .run = run_look_left,
};
const tokki_action_descriptor_t TOKKI_OLED_LOOK_RIGHT_ACTION = {
    .id = "oled.look_right", .display_name = "Look right",
    .device = TOKKI_DEVICE_OLED, .cancellable = false, .run = run_look_right,
};
const tokki_action_descriptor_t TOKKI_OLED_LOOK_UP_ACTION = {
    .id = "oled.look_up", .display_name = "Look up",
    .device = TOKKI_DEVICE_OLED, .cancellable = false, .run = run_look_up,
};
const tokki_action_descriptor_t TOKKI_OLED_LOOK_DOWN_ACTION = {
    .id = "oled.look_down", .display_name = "Look down",
    .device = TOKKI_DEVICE_OLED, .cancellable = false, .run = run_look_down,
};
const tokki_action_descriptor_t TOKKI_OLED_SLEEPY_ACTION = {
    .id = "oled.sleepy", .display_name = "Sleepy eyes",
    .device = TOKKI_DEVICE_OLED, .cancellable = false, .run = run_sleepy,
};

const tokki_action_descriptor_t TOKKI_OLED_HEART_ACTION = {
    .id = "oled.heart", .display_name = "Heart pulse",
    .device = TOKKI_DEVICE_OLED, .cancellable = false, .run = run_heart,
};
const tokki_action_descriptor_t TOKKI_OLED_EXCLAMATION_ACTION = {
    .id = "oled.exclamation", .display_name = "Exclamation mark",
    .device = TOKKI_DEVICE_OLED, .cancellable = false, .run = run_exclamation,
};