#include "tokki_idle.h"

#include "pet_eyes.h"
#include "tokki_oled.h"

typedef struct {
    pet_eye_expression_t expression;
    unsigned first_frame;
    unsigned frames;
} idle_phase_t;

static const idle_phase_t PHASES[] = {
    {PET_EYES_HAPPY, 0, 96},
    {PET_EYES_LOOK_LEFT, 0, 24},
    {PET_EYES_HAPPY, 0, 48},
    {PET_EYES_CURIOUS, 0, 48},
    {PET_EYES_HAPPY, 34, 9},
    {PET_EYES_LOOK_RIGHT, 0, 24},
    {PET_EYES_HAPPY, 0, 48},
};

void tokki_idle_reset(tokki_idle_t *idle)
{
    idle->phase = 0;
    idle->frame = 0;
}

esp_err_t tokki_idle_step(tokki_idle_t *idle)
{
    const idle_phase_t *phase = &PHASES[idle->phase];
    uint8_t framebuffer[TOKKI_OLED_FRAME_SIZE];
    pet_eyes_render(framebuffer, sizeof(framebuffer),
                     TOKKI_OLED_WIDTH, TOKKI_OLED_HEIGHT,
                     phase->expression, phase->first_frame + idle->frame);
    esp_err_t result = tokki_oled_draw_frame(framebuffer, sizeof(framebuffer));
    if (result != ESP_OK) {
        return result;
    }
    if (++idle->frame == phase->frames) {
        idle->frame = 0;
        idle->phase = (idle->phase + 1) % (sizeof(PHASES) / sizeof(PHASES[0]));
    }
    return ESP_OK;
}
