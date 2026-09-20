#include "tokki_idle.h"

#include "pet_eyes.h"
#include "tokki_neopixel.h"
#include "tokki_oled.h"

#define IDLE_BLINK_FIRST_FRAME 34
#define IDLE_BLINK_FRAMES 9
#define IDLE_NIGHT_SKY_FRAMES 48
#define IDLE_SLEEP_LAST_CLOSED_FRAME 39

typedef struct {
    pet_eye_expression_t expression;
    unsigned first_frame;
    unsigned frames;
    bool blink_after;
    bool night_sky_after;
} idle_phase_t;

static const idle_phase_t PHASES[] = {
    {.expression = PET_EYES_HAPPY, .first_frame = IDLE_BLINK_FIRST_FRAME, .frames = IDLE_BLINK_FRAMES},
    {.expression = PET_EYES_LOOK_LEFT, .frames = 24, .blink_after = true},
    {.expression = PET_EYES_LOOK_RIGHT, .frames = 24, .blink_after = true},
    {.expression = PET_EYES_LOOK_UP, .frames = 24},
    {.expression = PET_EYES_HAPPY, .frames = 48},
    {.expression = PET_EYES_CURIOUS, .frames = 48},
    {.expression = PET_EYES_LOVEY_DOVEY, .frames = 48},
    {.expression = PET_EYES_SHY, .frames = 48},
    {.expression = PET_EYES_SLEEPING, .frames = 48, .night_sky_after = true},
};

_Static_assert(sizeof(PHASES) / sizeof(PHASES[0]) == TOKKI_IDLE_EYE_VARIANTS,
               "Update the idle shuffle capacity when adding OLED phases");

static uint32_t next_random(uint32_t *state)
{
    uint32_t value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static void start_rest(tokki_idle_t *idle)
{
    idle->resting = true;
    idle->frame = 0;
    idle->frames = 10 + next_random(&idle->random_state) % 11;
}

static void start_expression(tokki_idle_t *idle)
{
    if (idle->next_phase == TOKKI_IDLE_EYE_VARIANTS) {
        for (unsigned i = 0; i < TOKKI_IDLE_EYE_VARIANTS; ++i) {
            idle->order[i] = i;
        }
        for (unsigned i = TOKKI_IDLE_EYE_VARIANTS - 1; i > 0; --i) {
            unsigned other = next_random(&idle->random_state) % (i + 1);
            unsigned swap = idle->order[i];
            idle->order[i] = idle->order[other];
            idle->order[other] = swap;
        }
        /* Avoid an immediate repeat across shuffled rounds. */
        if (idle->order[0] == idle->phase) {
            unsigned swap = idle->order[0];
            idle->order[0] = idle->order[1];
            idle->order[1] = swap;
        }
        idle->next_phase = 0;
    }
    idle->phase = idle->order[idle->next_phase++];
    const idle_phase_t *phase = &PHASES[idle->phase];
    idle->frames = phase->frames + (phase->blink_after ? IDLE_BLINK_FRAMES : 0) +
                   (phase->night_sky_after ? IDLE_NIGHT_SKY_FRAMES : 0);
    idle->resting = false;
    idle->frame = 0;
}

void tokki_idle_reset(tokki_idle_t *idle, uint32_t seed)
{
    *idle = (tokki_idle_t) {
        .random_state = seed != 0 ? seed : 0x6D2B79F5U,
        .phase = TOKKI_IDLE_EYE_VARIANTS,
        .next_phase = TOKKI_IDLE_EYE_VARIANTS,
    };
    start_rest(idle);
}

esp_err_t tokki_idle_step(tokki_idle_t *idle)
{
    pet_eye_expression_t expression = PET_EYES_HAPPY;
    bool night_sky = false;
    unsigned frame = 0;
    if (!idle->resting && idle->frame + 1 < idle->frames) {
        const idle_phase_t *phase = &PHASES[idle->phase];
        if (phase->blink_after && idle->frame >= phase->frames) {
            frame = IDLE_BLINK_FIRST_FRAME + idle->frame - phase->frames;
        } else if (phase->night_sky_after && idle->frame >= phase->frames) {
            night_sky = true;
            frame = idle->frame - phase->frames;
        } else {
            expression = phase->expression;
            frame = phase->first_frame + idle->frame;
            /* Stay asleep through the transition; manual sleeping still reopens. */
            if (phase->night_sky_after && frame > IDLE_SLEEP_LAST_CLOSED_FRAME) {
                frame = IDLE_SLEEP_LAST_CLOSED_FRAME;
            }
        }
    }
    uint8_t framebuffer[TOKKI_OLED_FRAME_SIZE];
    esp_err_t result = ESP_OK;
    if (night_sky) {
        result = tokki_oled_render_art(framebuffer, sizeof(framebuffer), TOKKI_OLED_ART_NIGHT_SKY, frame);
    } else {
        pet_eyes_render(framebuffer, sizeof(framebuffer),
                         TOKKI_OLED_WIDTH, TOKKI_OLED_HEIGHT,
                         expression, frame);
    }
    if (result == ESP_OK) {
        result = tokki_oled_draw_frame(framebuffer, sizeof(framebuffer));
    }
    if (result != ESP_OK) {
        return result;
    }
    if (++idle->frame == idle->frames) {
        if (idle->resting) {
            start_expression(idle);
        } else {
            start_rest(idle);
        }
    }
    return ESP_OK;
}

void tokki_idle_neopixel_reset(tokki_idle_neopixel_t *idle, uint32_t seed)
{
    *idle = (tokki_idle_neopixel_t) {
        .random_state = seed != 0 ? seed : 0xA341316CU,
        .resting = true,
    };
}

esp_err_t tokki_idle_neopixel_step(tokki_idle_neopixel_t *idle)
{
    esp_err_t result = idle->resting ? tokki_neopixel_set_color(0, 0, 0) :
                                     tokki_neopixel_fade_step(true, idle->frame, 16);
    if (result != ESP_OK) {
        return result;
    }
    if (idle->resting) {
        idle->delay_ms = 6000 + next_random(&idle->random_state) % 8001;
        idle->frame = 0;
        idle->resting = false;
    } else {
        idle->delay_ms = TOKKI_IDLE_FRAME_MS;
        if (++idle->frame == 33) {
            idle->resting = true;
        }
    }
    return ESP_OK;
}
