#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "freertos/task.h"
#include "tokki_gestures.h"
#include "tokki_led.h"
#include "tokki_neopixel.h"
#include "tokki_oled.h"
#include "pet_eyes.h"
#include "tokki_speaker.h"
#include "speaker_tone.h"

static unsigned color_calls;
static unsigned fail_color_call;
static unsigned elapsed_ms;
static uint8_t colors[129][3];
static unsigned oled_calls;
static unsigned fail_oled_call;
static uint8_t last_frame[TOKKI_OLED_FRAME_SIZE];
static tokki_speaker_sound_t last_sound;
static esp_err_t speaker_result;
static FILE *preview_output;
static bool record_preview;
static unsigned preview_actions;
static uint8_t bark_pcm[16636];

static void begin_preview(const char *id, unsigned duration, const char *kind)
{
    const tokki_action_descriptor_t *action = tokki_action_find(id);
    assert(action != NULL);
    assert(strpbrk(action->display_name, "\"\\\n\r\t") == NULL);
    fprintf(preview_output, "%s\"%s\":{\"name\":\"%s\",\"duration\":%u,\"kind\":\"%s\"",
            preview_actions++ == 0 ? "" : ",\n", id, action->display_name, duration, kind);
}

static void export_colors(const char *id, unsigned frame_ms)
{
    if (preview_output == NULL) {
        return;
    }
    begin_preview(id, elapsed_ms, "color");
    fprintf(preview_output, ",\"frameMs\":%u,\"colors\":[", frame_ms);
    for (unsigned index = 0; index < color_calls; ++index) {
        fprintf(preview_output, "%s[%u,%u,%u]", index == 0 ? "" : ",",
                colors[index][0], colors[index][1], colors[index][2]);
    }
    fputs("]}", preview_output);
}

esp_err_t tokki_speaker_play(tokki_speaker_sound_t sound)
{
    last_sound = sound;
    return speaker_result;
}

esp_err_t tokki_oled_draw_frame(const uint8_t *framebuffer, size_t size)
{
    assert(framebuffer != NULL && size == TOKKI_OLED_FRAME_SIZE);
    size_t lit = 0;
    for (size_t index = 0; index < size; ++index) {
        lit += framebuffer[index] != 0;
    }
    assert(lit > 0 && lit < size);
    if (record_preview) {
        fprintf(preview_output, "%s\"", oled_calls == 0 ? "" : ",");
        for (size_t index = 0; index < size; ++index) {
            fprintf(preview_output, "%02x", framebuffer[index]);
        }
        fputc('"', preview_output);
    }
    memcpy(last_frame, framebuffer, size);
    ++oled_calls;
    return oled_calls == fail_oled_call ? ESP_FAIL : ESP_OK;
}

void vTaskDelay(TickType_t ticks)
{
    elapsed_ms += ticks;
}

esp_err_t tokki_neopixel_set_color(uint8_t red, uint8_t green, uint8_t blue)
{
    assert(color_calls < 129);
    colors[color_calls][0] = red;
    colors[color_calls][1] = green;
    colors[color_calls][2] = blue;
    ++color_calls;
    return color_calls == fail_color_call ? ESP_FAIL : ESP_OK;
}

esp_err_t tokki_led_blink(uint32_t count, uint32_t on_ms, uint32_t off_ms)
{
    assert(count == 3 && on_ms == 300 && off_ms == 300);
    elapsed_ms += count * (on_ms + off_ms);
    return ESP_OK;
}

static void reset_output(void)
{
    color_calls = 0;
    fail_color_call = 0;
    elapsed_ms = 0;
    oled_calls = 0;
    fail_oled_call = 0;
    memset(colors, 0, sizeof(colors));
}

static void test_blink(const char *id, uint8_t red, uint8_t green)
{
    reset_output();
    assert(tokki_action_run(id) == ESP_OK);
    assert(color_calls == 6 && elapsed_ms == 1800);
    for (unsigned index = 0; index < 6; ++index) {
        assert(colors[index][0] == (index % 2 == 0 ? red : 0));
        assert(colors[index][1] == (index % 2 == 0 ? green : 0));
        assert(colors[index][2] == 0);
    }
    export_colors(id, 300);
    for (unsigned failure = 1; failure <= 6; ++failure) {
        reset_output();
        fail_color_call = failure;
        assert(tokki_action_run(id) == ESP_FAIL);
        assert(color_calls == failure);
        assert(elapsed_ms == (failure - 1) * 300);
    }
}

static void test_fade(const char *id, unsigned half_steps, bool teal)
{
    reset_output();
    assert(tokki_neopixel_fade_step(teal, 0, 0) == ESP_ERR_INVALID_ARG);
    assert(tokki_neopixel_fade_step(teal, 0, UINT_MAX) == ESP_ERR_INVALID_ARG);
    assert(tokki_neopixel_fade_step(teal, half_steps * 2 + 1, half_steps) == ESP_ERR_INVALID_ARG);
    assert(color_calls == 0 && elapsed_ms == 0);
    unsigned count = half_steps * 2 + 1;
    reset_output();
    assert(tokki_action_run(id) == ESP_OK);
    assert(color_calls == count && elapsed_ms == count * 60);
    for (unsigned index = 0; index < count; ++index) {
        unsigned distance = index <= half_steps ? index : half_steps * 2 - index;
        unsigned brightness = 32 * distance / half_steps;
        assert(colors[index][0] == 0);
        assert(colors[index][1] == (teal ? brightness : 0));
        assert(colors[index][2] == brightness);
    }
    export_colors(id, 60);
    for (unsigned failure = 1; failure <= count; ++failure) {
        reset_output();
        fail_color_call = failure;
        assert(tokki_action_run(id) == ESP_FAIL);
        assert(color_calls == failure && elapsed_ms == (failure - 1) * 60);
    }
}

static void test_rainbow(void)
{
    uint8_t original_colors[128][3];
    reset_output();
    assert(tokki_neopixel_rainbow(1) == ESP_OK);
    assert(color_calls == 128 && elapsed_ms == 2560);
    memcpy(original_colors, colors, sizeof(original_colors));
    reset_output();
    assert(tokki_action_run("neopixel.rainbow") == ESP_OK);
    assert(color_calls == 129 && elapsed_ms == 3840);
    assert(memcmp(original_colors, colors, sizeof(original_colors)) == 0);
    assert(colors[128][0] == 0 && colors[128][1] == 0 && colors[128][2] == 0);
    for (unsigned index = 0; index < color_calls; ++index) {
        for (unsigned channel = 0; channel < 3; ++channel) {
            assert(colors[index][channel] <= 32);
        }
    }
    for (unsigned failure = 1; failure <= 129; ++failure) {
        reset_output();
        fail_color_call = failure;
        assert(tokki_action_run("neopixel.rainbow") == ESP_FAIL);
        assert(color_calls == failure && elapsed_ms == (failure - 1) * 30);
    }
    reset_output();
    assert(tokki_neopixel_rainbow_timed(1, 0) == ESP_ERR_INVALID_ARG);
    assert(tokki_neopixel_rainbow_timed(1, 1001) == ESP_ERR_INVALID_ARG);
    assert(color_calls == 0 && elapsed_ms == 0);
}

static void test_oled(const char *id, unsigned frames, unsigned original_duration)
{
    unsigned duration = original_duration * 3 / 2;
    reset_output();
    if (preview_output != NULL) {
        begin_preview(id, duration, "oled");
        fputs(",\"frames\":[", preview_output);
        record_preview = true;
    }
    assert(tokki_action_run(id) == ESP_OK);
    record_preview = false;
    if (preview_output != NULL) {
        fputs("]}", preview_output);
    }
    assert(oled_calls == frames && elapsed_ms == duration);
    for (unsigned failure = 1; failure <= frames; ++failure) {
        reset_output();
        fail_oled_call = failure;
        assert(tokki_action_run(id) == ESP_FAIL);
        assert(oled_calls == failure);
        assert(elapsed_ms == (failure - 1) * (duration / frames));
    }
}

static bool eye_pixel(const uint8_t *frame, int x, int y)
{
    return (frame[(y / 8) * TOKKI_OLED_WIDTH + x] & (1U << (y % 8))) != 0;
}

typedef struct {
    int left;
    int right;
    int top;
    int bottom;
    unsigned lit;
} eye_bounds_t;

static eye_bounds_t eye_bounds(const uint8_t *frame, int eye)
{
    eye_bounds_t bounds = {128, -1, 64, -1, 0};
    for (int y = 0; y < 64; ++y) {
        for (int x = eye * 64; x < (eye + 1) * 64; ++x) {
            if (eye_pixel(frame, x, y)) {
                if (x < bounds.left) bounds.left = x;
                if (x > bounds.right) bounds.right = x;
                if (y < bounds.top) bounds.top = y;
                if (y > bounds.bottom) bounds.bottom = y;
                ++bounds.lit;
            }
        }
    }
    return bounds;
}

static int eye_column_top(const uint8_t *frame, int x)
{
    for (int y = 0; y < 64; ++y) {
        if (eye_pixel(frame, x, y)) {
            return y;
        }
    }
    assert(false);
    return -1;
}

static int pupil_center(const uint8_t *frame, bool vertical)
{
    int first = -1;
    int last = -1;
    int extent = vertical ? 20 : 16;
    for (int offset = -extent; offset <= extent; ++offset) {
        if (!eye_pixel(frame, vertical ? 40 : 40 + offset, vertical ? 32 + offset : 32)) {
            if (first == -1) first = offset + 32;
            last = offset + 32;
        }
    }
    assert(first != -1);
    return (first + last) / 2 - 32;
}

static void assert_only_eye_shapes(const uint8_t *frame)
{
    bool visited[128 * 64] = {false};
    int queue[128 * 64];
    unsigned shapes = 0;
    for (int pixel = 0; pixel < 128 * 64; ++pixel) {
        if (visited[pixel] || !eye_pixel(frame, pixel % 128, pixel / 128)) {
            continue;
        }
        size_t head = 0;
        size_t tail = 0;
        queue[tail++] = pixel;
        visited[pixel] = true;
        while (head < tail) {
            int current = queue[head++];
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int x = current % 128 + dx;
                    int y = current / 128 + dy;
                    if (x < 0 || x >= 128 || y < 0 || y >= 64) continue;
                    int next = y * 128 + x;
                    if (!visited[next] && eye_pixel(frame, x, y)) {
                        visited[next] = true;
                        queue[tail++] = next;
                    }
                }
            }
        }
        /* Two connected eyes, with only tiny catchlights as separate islands. */
        if (tail > 5) ++shapes;
    }
    assert(shapes == 2);
}

static void test_cartoon_eyes(void)
{
    uint8_t neutral[TOKKI_OLED_FRAME_SIZE];
    uint8_t frame[TOKKI_OLED_FRAME_SIZE];
    uint8_t mirror[TOKKI_OLED_FRAME_SIZE];
    pet_eyes_render(neutral, sizeof(neutral), 128, 64, PET_EYES_HAPPY, 0);
    eye_bounds_t base = eye_bounds(neutral, 0);
    assert(base.left == 23 && base.right == 57 && base.top == 10 && base.bottom == 54);

    pet_eyes_render(frame, sizeof(frame), 128, 64, PET_EYES_HAPPY, 12);
    eye_bounds_t smile = eye_bounds(frame, 0);
    assert(smile.bottom - smile.top <= 12 && smile.right - smile.left >= 38);
    assert(smile.lit < base.lit / 3);
    assert(eye_column_top(frame, 40) + 4 < eye_column_top(frame, 22));
    pet_eyes_render(mirror, sizeof(mirror), 128, 64, PET_EYES_HAPPY, 20);
    assert(memcmp(frame, mirror, sizeof(frame)) == 0);
    pet_eyes_render(frame, sizeof(frame), 128, 64, PET_EYES_HAPPY, 47);
    assert(memcmp(frame, neutral, sizeof(frame)) == 0);

    unsigned previous_lit = base.lit + 1;
    for (unsigned phase = 0; phase <= 4; ++phase) {
        pet_eyes_render(frame, sizeof(frame), 128, 64, PET_EYES_HAPPY, 34 + phase);
        pet_eyes_render(mirror, sizeof(mirror), 128, 64, PET_EYES_HAPPY, 42 - phase);
        assert(memcmp(frame, mirror, sizeof(frame)) == 0);
        eye_bounds_t bounds = eye_bounds(frame, 0);
        assert(bounds.lit < previous_lit);
        previous_lit = bounds.lit;
    }
    for (unsigned phase = 34; phase <= 42; ++phase) {
        pet_eyes_render(frame, sizeof(frame), 128, 64, PET_EYES_WINK, phase);
        for (int y = 0; y < 64; ++y) {
            for (int x = 0; x < 64; ++x) {
                assert(eye_pixel(frame, x, y) == eye_pixel(neutral, x, y));
            }
        }
    }

    pet_eyes_render(frame, sizeof(frame), 128, 64, PET_EYES_SURPRISED, 0);
    assert(memcmp(frame, neutral, sizeof(frame)) == 0);
    pet_eyes_render(frame, sizeof(frame), 128, 64, PET_EYES_SURPRISED, 4);
    eye_bounds_t overshoot = eye_bounds(frame, 0);
    pet_eyes_render(frame, sizeof(frame), 128, 64, PET_EYES_SURPRISED, 8);
    eye_bounds_t surprise = eye_bounds(frame, 0);
    assert(overshoot.top < surprise.top && overshoot.bottom > surprise.bottom);
    assert(surprise.bottom - surprise.top >= base.bottom - base.top + 10);
    unsigned pin_pixels = 0;
    for (int y = 20; y <= 44; ++y) {
        for (int x = 30; x <= 50; ++x) {
            if (!eye_pixel(frame, x, y)) ++pin_pixels;
        }
    }
    assert(pin_pixels == 13);
    pet_eyes_render(mirror, sizeof(mirror), 128, 64, PET_EYES_SURPRISED, 38);
    assert(memcmp(frame, mirror, sizeof(frame)) == 0);

    pet_eyes_render(frame, sizeof(frame), 128, 64, PET_EYES_CURIOUS, 16);
    eye_bounds_t big = eye_bounds(frame, 0);
    eye_bounds_t small = eye_bounds(frame, 1);
    assert(big.bottom - big.top >= small.bottom - small.top + 10);
    assert(big.lit > small.lit * 3 / 2);
    assert(!eye_pixel(frame, 49, 32) && eye_pixel(frame, 34, 32));
    assert(!eye_pixel(frame, 96, 32) && eye_pixel(frame, 86, 32));
    pet_eyes_render(mirror, sizeof(mirror), 128, 64, PET_EYES_CURIOUS, 35);
    assert(!eye_pixel(mirror, 33, 32) && eye_pixel(mirror, 47, 32));
    assert(!eye_pixel(mirror, 81, 32) && eye_pixel(mirror, 92, 32));

    pet_eyes_render(frame, sizeof(frame), 128, 64, PET_EYES_SAD, 12);
    eye_bounds_t sad = eye_bounds(frame, 0);
    assert(sad.top > base.top && sad.bottom < base.bottom);
    assert(eye_column_top(frame, 32) > eye_column_top(frame, 48));
    assert(!eye_pixel(frame, 42, 46) && eye_pixel(frame, 42, 25));

    int previous_top = base.top - 1;
    for (unsigned phase = 0; phase <= 12; phase += 4) {
        pet_eyes_render(frame, sizeof(frame), 128, 64, PET_EYES_SLEEPY, phase);
        int top = eye_column_top(frame, 40);
        assert(top > previous_top);
        previous_top = top;
    }
    pet_eyes_render(frame, sizeof(frame), 128, 64, PET_EYES_SLEEPY, 13);
    eye_bounds_t sleepy = eye_bounds(frame, 0);
    assert(sleepy.bottom - sleepy.top < 18 && sleepy.lit < base.lit / 2);

    for (int expression = PET_EYES_LOOK_LEFT; expression <= PET_EYES_LOOK_DOWN; ++expression) {
        bool vertical = expression >= PET_EYES_LOOK_UP;
        int direction = expression == PET_EYES_LOOK_LEFT || expression == PET_EYES_LOOK_UP ? -1 : 1;
        pet_eyes_render(frame, sizeof(frame), 128, 64, (pet_eye_expression_t) expression, 4);
        int peak = pupil_center(frame, vertical) * direction;
        pet_eyes_render(frame, sizeof(frame), 128, 64, (pet_eye_expression_t) expression, 8);
        int settled = pupil_center(frame, vertical) * direction;
        assert(peak > settled && settled == (vertical ? 6 : 8));
        for (unsigned phase = 0; phase < 24; ++phase) {
            pet_eyes_render(frame, sizeof(frame), 128, 64, (pet_eye_expression_t) expression, phase);
            eye_bounds_t bounds = eye_bounds(frame, 0);
            assert(bounds.left == base.left && bounds.right == base.right);
            assert(bounds.top == base.top && bounds.bottom == base.bottom);
        }
        assert(memcmp(frame, neutral, sizeof(frame)) == 0);
    }
    for (int expression = PET_EYES_HAPPY; expression <= PET_EYES_SHY; ++expression) {
        for (unsigned phase = 0; phase < 48; ++phase) {
            pet_eyes_render(frame, sizeof(frame), 128, 64, (pet_eye_expression_t) expression, phase);
            assert_only_eye_shapes(frame);
            pet_eyes_render(mirror, sizeof(mirror), 128, 64, (pet_eye_expression_t) expression, phase + 48);
            assert(memcmp(frame, mirror, sizeof(frame)) == 0);
        }
    }
}

static void test_affectionate_eyes(void)
{
    uint8_t neutral[TOKKI_OLED_FRAME_SIZE];
    uint8_t love[TOKKI_OLED_FRAME_SIZE];
    uint8_t pulse[TOKKI_OLED_FRAME_SIZE];
    uint8_t shy[TOKKI_OLED_FRAME_SIZE];
    uint8_t peek[TOKKI_OLED_FRAME_SIZE];
    pet_eyes_render(neutral, sizeof(neutral), 128, 64, PET_EYES_HAPPY, 0);
    pet_eyes_render(love, sizeof(love), 128, 64, PET_EYES_LOVEY_DOVEY, 0);
    pet_eyes_render(pulse, sizeof(pulse), 128, 64, PET_EYES_LOVEY_DOVEY, 6);
    for (int eye = 0; eye < 2; ++eye) {
        int center = 40 + eye * 48;
        /* Two lobes, a clear notch, and a single-pixel pointed tip. */
        assert(!eye_pixel(love, center - 4, 26) && !eye_pixel(love, center + 4, 26));
        assert(eye_pixel(love, center, 26));
        assert(!eye_pixel(love, center, 40));
        assert(eye_pixel(love, center - 1, 40) && eye_pixel(love, center + 1, 40));
        assert(eye_pixel(love, center - 10, 29) && !eye_pixel(pulse, center - 10, 29));
        assert(eye_pixel(love, center + 10, 29) && !eye_pixel(pulse, center + 10, 29));
        eye_bounds_t small = eye_bounds(love, eye);
        eye_bounds_t big = eye_bounds(pulse, eye);
        assert(big.top < small.top && big.bottom > small.bottom);
    }
    pet_eyes_render(peek, sizeof(peek), 128, 64, PET_EYES_LOVEY_DOVEY, 26);
    assert(memcmp(pulse, peek, sizeof(pulse)) == 0);
    pet_eyes_render(peek, sizeof(peek), 128, 64, PET_EYES_LOVEY_DOVEY, 38);
    pet_eyes_render(pulse, sizeof(pulse), 128, 64, PET_EYES_HAPPY, 38);
    assert(memcmp(pulse, peek, sizeof(pulse)) == 0);

    pet_eyes_render(shy, sizeof(shy), 128, 64, PET_EYES_SHY, 12);
    pet_eyes_render(peek, sizeof(peek), 128, 64, PET_EYES_SHY, 24);
    eye_bounds_t base = eye_bounds(neutral, 0);
    eye_bounds_t lowered = eye_bounds(shy, 0);
    eye_bounds_t peeking = eye_bounds(peek, 0);
    assert(lowered.top > peeking.top && peeking.top > base.top);
    assert(lowered.lit < peeking.lit && peeking.lit < base.lit);
    assert(eye_pixel(shy, 34, 40) && !eye_pixel(shy, 50, 40));
    assert(eye_pixel(shy, 94, 40) && !eye_pixel(shy, 78, 40));
    assert(!eye_pixel(shy, 45, 46) && eye_pixel(shy, 45, 29));
    assert(eye_column_top(shy, 40) != eye_column_top(shy, 88));
    pet_eyes_render(peek, sizeof(peek), 128, 64, PET_EYES_SAD, 12);
    assert(memcmp(shy, peek, sizeof(shy)) != 0);
    pet_eyes_render(peek, sizeof(peek), 128, 64, PET_EYES_SLEEPY, 12);
    assert(memcmp(shy, peek, sizeof(shy)) != 0);

    const char *ids[] = {"oled.lovey_dovey", "oled.shy"};
    const pet_eye_expression_t expressions[] = {PET_EYES_LOVEY_DOVEY, PET_EYES_SHY};
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i) {
        pet_eyes_render(peek, sizeof(peek), 128, 64, expressions[i], 46);
        assert(memcmp(peek, neutral, sizeof(peek)) == 0);
        reset_output();
        assert(tokki_action_run(ids[i]) == ESP_OK);
        assert(oled_calls == 48 && elapsed_ms == 4320);
        assert(memcmp(last_frame, neutral, sizeof(neutral)) == 0);
    }
}

static void test_sleeping_eyes(void)
{
    uint8_t neutral[TOKKI_OLED_FRAME_SIZE];
    uint8_t frame[TOKKI_OLED_FRAME_SIZE];
    uint8_t repeated[TOKKI_OLED_FRAME_SIZE];
    pet_eyes_render(neutral, sizeof(neutral), 128, 64, PET_EYES_HAPPY, 0);
    unsigned previous_lit = eye_bounds(neutral, 0).lit + 1;
    for (unsigned phase = 0; phase <= 8; ++phase) {
        pet_eyes_render(frame, sizeof(frame), 128, 64, PET_EYES_SLEEPING, phase);
        unsigned lit = eye_bounds(frame, 0).lit;
        assert(lit <= previous_lit);
        previous_lit = lit;
    }
    for (unsigned phase = 0; phase < 48; ++phase) {
        pet_eyes_render(frame, sizeof(frame), 128, 64, PET_EYES_SLEEPING, phase);
        pet_eyes_render(repeated, sizeof(repeated), 128, 64, PET_EYES_SLEEPING, phase + 48);
        assert(memcmp(frame, repeated, sizeof(frame)) == 0);
        if (phase == 0 || phase >= 46) {
            assert(memcmp(frame, neutral, sizeof(frame)) == 0);
        }
        if (phase >= 8 && phase <= 39) {
            eye_bounds_t closed = eye_bounds(frame, 0);
            assert(closed.bottom - closed.top <= 12 && closed.top >= 28);
            assert(closed.lit < eye_bounds(neutral, 0).lit / 3);
            for (int y = 28; y < 64; ++y) {
                for (int x = 16; x <= 63; ++x) {
                    assert(eye_pixel(frame, x, y) == eye_pixel(frame, x + 48, y));
                }
            }
            for (unsigned mark = 0; mark < 3; ++mark) {
                int age = (int) ((phase - 8 + mark * 8) % 24);
                int x = 78 + age;
                int y = 22 - age * 3 / 4;
                int size = 3 + age / 6;
                for (int dx = 0; dx <= size; ++dx) {
                    assert(eye_pixel(frame, x + dx, y));
                    assert(eye_pixel(frame, x + dx, y + size + 1));
                }
                assert(eye_pixel(frame, x + size / 2, y + (size + 1) / 2));
            }
            pet_eyes_render(repeated, sizeof(repeated), 128, 64, PET_EYES_SLEEPING, phase + 1);
            assert(memcmp(frame, repeated, sizeof(frame)) != 0);
        }
    }
    pet_eyes_render(frame, sizeof(frame), 128, 64, PET_EYES_SLEEPING, UINT_MAX);
    pet_eyes_render(repeated, sizeof(repeated), 128, 64, PET_EYES_SLEEPING, UINT_MAX % 48);
    assert(memcmp(frame, repeated, sizeof(frame)) == 0);
    reset_output();
    assert(tokki_action_run("oled.sleeping") == ESP_OK);
    assert(oled_calls == 48 && elapsed_ms == 4320);
    assert(memcmp(last_frame, neutral, sizeof(neutral)) == 0);
}

static void test_renderers(void)
{
    uint8_t guarded[TOKKI_OLED_FRAME_SIZE + 2];
    memset(guarded, 0xA5, sizeof(guarded));
    for (unsigned frame = 0; frame < 64; ++frame) {
        for (int expression = PET_EYES_HAPPY; expression <= PET_EYES_SLEEPING; ++expression) {
            pet_eyes_render(guarded + 1, TOKKI_OLED_FRAME_SIZE, 128, 64,
                             (pet_eye_expression_t) expression, frame);
            assert(guarded[0] == 0xA5 && guarded[sizeof(guarded) - 1] == 0xA5);
        }
        for (int art = TOKKI_OLED_ART_DRINK_WATER; art <= TOKKI_OLED_ART_SUNRISE; ++art) {
            assert(tokki_oled_render_art(guarded + 1, TOKKI_OLED_FRAME_SIZE,
                                          (tokki_oled_art_t) art, frame) == ESP_OK);
            assert(guarded[0] == 0xA5 && guarded[sizeof(guarded) - 1] == 0xA5);
        }
    }
    assert(tokki_oled_render_art(NULL, TOKKI_OLED_FRAME_SIZE, TOKKI_OLED_ART_FIRE, 0) == ESP_ERR_INVALID_ARG);
    assert(tokki_oled_render_art(guarded, 1, TOKKI_OLED_ART_FIRE, 0) == ESP_ERR_INVALID_ARG);
    assert(tokki_oled_render_art(guarded, TOKKI_OLED_FRAME_SIZE, (tokki_oled_art_t) 99, 0) == ESP_ERR_INVALID_ARG);
    uint8_t happy[TOKKI_OLED_FRAME_SIZE];
    pet_eyes_render(happy, sizeof(happy), 128, 64, PET_EYES_HAPPY, 0);
    pet_eyes_render(guarded + 1, TOKKI_OLED_FRAME_SIZE, 128, 64, PET_EYES_SURPRISED, 8);
    assert(memcmp(happy, guarded + 1, sizeof(happy)) != 0);
    reset_output();
    assert(tokki_action_run("oled.blink") == ESP_OK);
    pet_eyes_render(happy, sizeof(happy), 128, 64, PET_EYES_HAPPY, 42);
    assert(memcmp(happy, last_frame, sizeof(happy)) == 0);
    uint8_t open_wink[TOKKI_OLED_FRAME_SIZE];
    uint8_t closed_wink[TOKKI_OLED_FRAME_SIZE];
    pet_eyes_render(open_wink, sizeof(open_wink), 128, 64, PET_EYES_WINK, 34);
    pet_eyes_render(closed_wink, sizeof(closed_wink), 128, 64, PET_EYES_WINK, 38);
    unsigned left_lit = 0;
    unsigned right_lit = 0;
    for (unsigned row = 22; row < 48; ++row) {
        for (unsigned column = 20; column < 60; ++column) {
            left_lit += (closed_wink[(row / 8) * 128 + column] >> (row % 8)) & 1U;
            right_lit += (closed_wink[(row / 8) * 128 + column + 48] >> (row % 8)) & 1U;
        }
    }
    assert(left_lit > right_lit * 2 && right_lit > 0);
    assert(memcmp(open_wink, closed_wink, sizeof(open_wink)) != 0);
    reset_output();
    assert(tokki_action_run("oled.wink") == ESP_OK);
    pet_eyes_render(happy, sizeof(happy), 128, 64, PET_EYES_HAPPY, 0);
    assert(memcmp(happy, last_frame, sizeof(happy)) == 0);
}

static void test_glances(void)
{
    const char *ids[] = {"oled.look_left", "oled.look_right", "oled.look_up", "oled.look_down", "oled.sleepy"};
    uint8_t happy[TOKKI_OLED_FRAME_SIZE];
    uint8_t frames[5][TOKKI_OLED_FRAME_SIZE];
    pet_eyes_render(happy, sizeof(happy), 128, 64, PET_EYES_HAPPY, 0);
    for (size_t index = 0; index < 5; ++index) {
        reset_output();
        assert(tokki_action_run(ids[index]) == ESP_OK);
        assert(memcmp(happy, last_frame, sizeof(happy)) == 0);
        pet_eyes_render(frames[index], sizeof(frames[index]), 128, 64,
                         (pet_eye_expression_t) (PET_EYES_LOOK_LEFT + index), 8);
        assert(memcmp(happy, frames[index], sizeof(happy)) != 0);
        for (size_t other = 0; other < index; ++other) {
            assert(memcmp(frames[index], frames[other], sizeof(happy)) != 0);
        }
    }
    assert(!eye_pixel(frames[0], 30, 32) && eye_pixel(frames[1], 30, 32));
    assert(!eye_pixel(frames[1], 50, 32) && eye_pixel(frames[0], 50, 32));
}

static void test_restoring_art(void)
{
    uint8_t happy[TOKKI_OLED_FRAME_SIZE];
    uint8_t first[TOKKI_OLED_FRAME_SIZE];
    uint8_t later[TOKKI_OLED_FRAME_SIZE];
    pet_eyes_render(happy, sizeof(happy), 128, 64, PET_EYES_HAPPY, 0);
    const char *ids[] = {"oled.checkmark", "oled.thinking", "oled.heart", "oled.exclamation",
                         "oled.night_sky", "oled.sunrise"};
    for (size_t index = 0; index < sizeof(ids) / sizeof(ids[0]); ++index) {
        reset_output();
        assert(tokki_action_run(ids[index]) == ESP_OK);
        assert(memcmp(happy, last_frame, sizeof(happy)) == 0);
        tokki_oled_art_t art = (tokki_oled_art_t) (TOKKI_OLED_ART_CHECKMARK + index);
        assert(tokki_oled_render_art(first, sizeof(first), art, 0) == ESP_OK);
        assert(tokki_oled_render_art(later, sizeof(later), art, 8) == ESP_OK);
        assert(memcmp(first, later, sizeof(first)) != 0);
    }
    assert(tokki_oled_render_art(first, sizeof(first), TOKKI_OLED_ART_THINKING, 12) == ESP_OK);
    assert(tokki_oled_render_art(later, sizeof(later), TOKKI_OLED_ART_THINKING, 18) == ESP_OK);
    assert(memcmp(first, later, sizeof(first)) == 0);
}

static void test_sky_art(void)
{
    uint8_t first[TOKKI_OLED_FRAME_SIZE];
    uint8_t later[TOKKI_OLED_FRAME_SIZE];
    uint8_t guarded[TOKKI_OLED_FRAME_SIZE + 2];
    memset(guarded, 0xA5, sizeof(guarded));
    uint32_t night_hash = UINT32_C(2166136261);
    uint32_t sunrise_hash = UINT32_C(2166136261);
    const int stars[][2] = {
        {10, 12}, {29, 8}, {52, 16}, {75, 8}, {17, 33},
        {38, 26}, {67, 35}, {86, 42}, {113, 38}, {46, 43},
    };
    for (unsigned frame = 0; frame < 48; ++frame) {
        assert(tokki_oled_render_art(first, sizeof(first), TOKKI_OLED_ART_NIGHT_SKY, frame) == ESP_OK);
        if (frame < 47) {
            for (size_t i = 0; i < sizeof(first); ++i) {
                night_hash = (night_hash ^ first[i]) * UINT32_C(16777619);
            }
        }
        assert(tokki_oled_render_art(later, sizeof(later), TOKKI_OLED_ART_NIGHT_SKY, frame + 48) == ESP_OK);
        assert(memcmp(first, later, sizeof(first)) == 0);
        for (size_t star = 0; star < sizeof(stars) / sizeof(stars[0]); ++star) {
            assert(eye_pixel(first, stars[star][0], stars[star][1]));
        }
        assert(eye_pixel(first, 91, 17) && !eye_pixel(first, 102, 17));
        assert(eye_pixel(first, 24, 55) && eye_pixel(first, 0, 61));
        assert(eye_bounds(first, 0).lit + eye_bounds(first, 1).lit < 600);
    }
    assert(tokki_oled_render_art(first, sizeof(first), TOKKI_OLED_ART_NIGHT_SKY, 0) == ESP_OK);
    assert(tokki_oled_render_art(later, sizeof(later), TOKKI_OLED_ART_NIGHT_SKY, 12) == ESP_OK);
    assert(!eye_pixel(first, 8, 12) && eye_pixel(later, 8, 12));
    for (unsigned frame = 20; frame < 32; ++frame) {
        int x = 26 + ((int) frame - 20) * 4;
        int y = 19 + (int) frame - 20;
        assert(tokki_oled_render_art(first, sizeof(first), TOKKI_OLED_ART_NIGHT_SKY, frame) == ESP_OK);
        assert(eye_pixel(first, x, y) && eye_pixel(first, x - 6, y - 2));
    }
    assert(tokki_oled_render_art(first, sizeof(first), TOKKI_OLED_ART_NIGHT_SKY, 32) == ESP_OK);
    assert(!eye_pixel(first, 70, 30));

    int previous_top = 48;
    for (unsigned frame = 0; frame < 64; ++frame) {
        assert(tokki_oled_render_art(first, sizeof(first), TOKKI_OLED_ART_SUNRISE, frame) == ESP_OK);
        if (frame < 63) {
            for (size_t i = 0; i < sizeof(first); ++i) {
                sunrise_hash = (sunrise_hash ^ first[i]) * UINT32_C(16777619);
            }
        }
        int top = eye_column_top(first, 64);
        assert(top <= previous_top && top >= 7);
        previous_top = top;
        for (int x = 8; x <= 119; ++x) {
            assert(eye_pixel(first, x, 48) && eye_pixel(first, x, 49));
        }
        for (int y = 50; y < 64; ++y) {
            for (int x = 0; x < 128; ++x) {
                bool reflection = (y == 54 && x >= 45 && x <= 83) ||
                                  (y == 59 && x >= 53 && x <= 75);
                assert(eye_pixel(first, x, y) == reflection);
            }
        }
        if (frame == 0) {
            for (int y = 0; y < 48; ++y) {
                for (int x = 0; x < 128; ++x) assert(!eye_pixel(first, x, y));
            }
        }
        if (frame >= 42) {
            assert(eye_pixel(first, 64, 18) && eye_pixel(first, 64, 42));
            assert(eye_pixel(first, 52, 30) && eye_pixel(first, 76, 30));
            assert(eye_pixel(first, 64, 7) && !eye_pixel(first, 64, 45));
            assert(tokki_oled_render_art(later, sizeof(later), TOKKI_OLED_ART_SUNRISE, 42) == ESP_OK);
            assert(memcmp(first, later, sizeof(first)) == 0);
        }
    }
    // The desktop pixel-preview tests pin these same full-sequence fingerprints.
    assert(night_hash == UINT32_C(0xa5d255a7));
    assert(sunrise_hash == UINT32_C(0x44639cec));
    for (int art = TOKKI_OLED_ART_NIGHT_SKY; art <= TOKKI_OLED_ART_SUNRISE; ++art) {
        assert(tokki_oled_render_art(guarded + 1, TOKKI_OLED_FRAME_SIZE, (tokki_oled_art_t) art, UINT_MAX) == ESP_OK);
        assert(guarded[0] == 0xA5 && guarded[sizeof(guarded) - 1] == 0xA5);
        unsigned bounded = art == TOKKI_OLED_ART_NIGHT_SKY ? UINT_MAX % 48 : 42;
        assert(tokki_oled_render_art(later, sizeof(later), (tokki_oled_art_t) art, bounded) == ESP_OK);
        assert(memcmp(guarded + 1, later, sizeof(later)) == 0);
    }
}

static void export_tone(const char *id, tokki_speaker_sound_t sound)
{
    if (preview_output == NULL) {
        return;
    }
    size_t count = 0;
    const speaker_tone_step_t *steps = speaker_sound_steps(sound, &count);
    unsigned duration = 500;
    for (size_t index = 0; index < count; ++index) {
        duration += steps[index].duration_ms + steps[index].silence_ms;
    }
    begin_preview(id, duration, "tone");
    fprintf(preview_output, ",\"sampleRate\":%u,\"pcm\":\"", SPEAKER_SAMPLE_RATE_HZ);
    for (unsigned sample = 0; sample < SPEAKER_SAMPLE_RATE_HZ / 4; ++sample) {
        fputs("0000", preview_output);
    }
    for (size_t index = 0; index < count; ++index) {
        uint32_t phase = 0;
        unsigned frames = SPEAKER_SAMPLE_RATE_HZ * steps[index].duration_ms / 1000;
        for (unsigned frame = 0; frame < frames; ++frame) {
            unsigned frequency = speaker_step_frequency(&steps[index], frame, frames);
            int16_t sample = speaker_tone_scaled_sample(frame, frames, phase, steps[index].gain_percent);
            assert(sample >= -6553 && sample <= 6553);
            fprintf(preview_output, "%02x%02x", (unsigned) ((uint16_t) sample & 255),
                    (unsigned) ((uint16_t) sample >> 8));
            phase += (uint32_t) (((uint64_t) frequency << 32) / SPEAKER_SAMPLE_RATE_HZ);
        }
        for (unsigned frame = 0; frame < SPEAKER_SAMPLE_RATE_HZ * steps[index].silence_ms / 1000; ++frame) {
            fputs("0000", preview_output);
        }
    }
    for (unsigned sample = 0; sample < SPEAKER_SAMPLE_RATE_HZ / 4; ++sample) {
        fputs("0000", preview_output);
    }
    fputs("\"}", preview_output);
}

static void test_speaker(void)
{
    const char *ids[] = {"speaker.drink_water", "speaker.chirp", "speaker.alert", "speaker.chime", "speaker.ping",
                         "speaker.dog_bark",
                         "speaker.bubble", "speaker.whistle", "speaker.tone_low", "speaker.tone_mid",
                         "speaker.question", "speaker.tone_high", "speaker.sparkle", "speaker.trill",
                         "speaker.knock", "speaker.sonar", "speaker.tone_rise"};
    for (size_t index = 0; index < sizeof(ids) / sizeof(ids[0]); ++index) {
        speaker_result = ESP_OK;
        assert(tokki_action_run(ids[index]) == ESP_OK);
        assert(last_sound == (tokki_speaker_sound_t) index);
        if (index > 0 && last_sound != TOKKI_SPEAKER_DOG_BARK) {
            export_tone(ids[index], (tokki_speaker_sound_t) index);
        }
        speaker_result = ESP_FAIL;
        assert(tokki_action_run(ids[index]) == ESP_FAIL);
    }
    const char *removed[] = {"speaker.sigh", "speaker.boing", "speaker.downstep", "speaker.bark"};
    for (size_t index = 0; index < sizeof(removed) / sizeof(removed[0]); ++index) {
        assert(tokki_action_find(removed[index]) == NULL);
        assert(tokki_action_run(removed[index]) == ESP_ERR_NOT_FOUND);
    }
    size_t step_count = 99;
    assert(speaker_sound_steps(TOKKI_SPEAKER_CHIME, NULL) == NULL);
    assert(speaker_sound_steps((tokki_speaker_sound_t) 99, &step_count) == NULL && step_count == 0);
    assert(speaker_sound_steps(TOKKI_SPEAKER_DOG_BARK, &step_count) == NULL && step_count == 0);
    assert(TOKKI_SPEAKER_DOG_BARK == 5);
    const unsigned expected_duration[] = {0, 320, 180, 460, 100, 0, 90, 280, 300, 300, 320, 300, 330, 260, 230, 350, 720};
    const unsigned expected_start[] = {0, 1800, 660, 784, 1320, 0, 1200, 900, 440, 660, 700, 880, 1047, 1400, 500, 880, 440};
    const unsigned expected_end[] = {0, 3000, 660, 784, 1320, 0, 480, 2100, 440, 660, 700, 880, 1047, 1700, 200, 880, 440};
    const size_t expected_steps[] = {0, 2, 1, 2, 1, 0, 1, 1, 1, 1, 2, 1, 3, 3, 2, 2, 3};
    for (int sound = TOKKI_SPEAKER_CHIRP; sound <= TOKKI_SPEAKER_TONE_RISE; ++sound) {
        if (sound == TOKKI_SPEAKER_DOG_BARK) {
            continue;
        }
        const speaker_tone_step_t *steps = speaker_sound_steps((tokki_speaker_sound_t) sound, &step_count);
        assert(steps != NULL && step_count == expected_steps[sound]);
        assert(steps[0].start_hz == expected_start[sound]);
        assert(steps[0].end_hz == expected_end[sound]);
        unsigned duration = 0;
        for (size_t index = 0; index < step_count; ++index) {
            unsigned start = steps[index].start_hz;
            unsigned end = steps[index].end_hz;
            assert(start > 0 && start < SPEAKER_SAMPLE_RATE_HZ / 2);
            assert(end > 0 && end < SPEAKER_SAMPLE_RATE_HZ / 2);
            assert(steps[index].gain_percent == (sound == TOKKI_SPEAKER_SONAR && index == 1 ? 40U : 100U));
            if (sound == TOKKI_SPEAKER_TONE_RISE) {
                const unsigned frequencies[] = {440, 660, 880};
                assert(start == frequencies[index] && end == start);
                assert(steps[index].duration_ms == 200);
                assert(steps[index].silence_ms == (index < 2 ? 60U : 0U));
            }
            duration += steps[index].duration_ms + steps[index].silence_ms;
            unsigned frames = SPEAKER_SAMPLE_RATE_HZ * steps[index].duration_ms / 1000;
            unsigned previous = start;
            for (unsigned frame = 0; frame < frames; ++frame) {
                unsigned frequency = speaker_step_frequency(&steps[index], frame, frames);
                assert(frequency >= (start < end ? start : end));
                assert(frequency <= (start > end ? start : end));
                assert(start <= end ? frequency >= previous : frequency <= previous);
                if (end >= start) {
                    assert(frequency == start + (end - start) * frame / frames);
                }
                previous = frequency;
                for (uint32_t phase_index = 0; phase_index < 32; ++phase_index) {
                    int16_t sample = speaker_tone_scaled_sample(frame, frames, phase_index << 27, steps[index].gain_percent);
                    int16_t full_sample = speaker_tone_sample(frame, frames, phase_index << 27);
                    assert(sample == (int32_t) full_sample * (int32_t) steps[index].gain_percent / 100);
                    assert(sample >= -6553 && sample <= 6553);
                    if (frame == 0 || frame == frames - 1) {
                        assert(sample == 0);
                    }
                }
            }
        }
        assert(duration == expected_duration[sound]);
        if (sound >= TOKKI_SPEAKER_BUBBLE) {
            assert(duration + 500 <= 1500);
        }
        if (sound == TOKKI_SPEAKER_CHIME) {
            assert(step_count == 2 && steps[1].start_hz > steps[0].start_hz);
            assert(steps[0].duration_ms + steps[1].duration_ms == 400);
        }
    }
    const speaker_tone_step_t falling = {1200, 480, 90, 0, 100};
    assert(speaker_step_frequency(&falling, 720, 1440) == 840);
    assert(speaker_step_frequency(&falling, 0, 0) == 480);
    assert(speaker_step_frequency(&falling, 0, 1440) == 1200);
    assert(speaker_step_frequency(&falling, 1440, 1440) == 480);
    assert(speaker_step_frequency(&falling, 1441, 1440) == 480);
    assert(speaker_tone_scaled_sample(500, 1920, 8U << 27, 0) == 0);
    assert(speaker_tone_scaled_sample(500, 1920, 8U << 27, UINT32_MAX) == speaker_tone_sample(500, 1920, 8U << 27));
    for (unsigned frame = 0; frame < 4800; ++frame) {
        for (uint32_t phase_index = 0; phase_index < 32; ++phase_index) {
            int16_t sample = speaker_tone_sample(frame, 4800, phase_index << 27);
            assert(sample >= -6553 && sample <= 6553);
            if (frame == 0 || frame == 4799) {
                assert(sample == 0);
            }
        }
    }
    assert(speaker_tone_sample(0, 0, UINT32_MAX) == 0);
    assert(speaker_tone_sample(4800, 4800, UINT32_MAX) == 0);
    for (uint32_t phase_index = 0; phase_index < 16; ++phase_index) {
        assert(speaker_tone_sample(2400, 4800, phase_index << 27) ==
               -speaker_tone_sample(2400, 4800, (phase_index + 16) << 27));
    }
}

static uint32_t wav_read_u32(const uint8_t *value)
{
    return (uint32_t) value[0] | ((uint32_t) value[1] << 8) |
           ((uint32_t) value[2] << 16) | ((uint32_t) value[3] << 24);
}

static int16_t bark_sample(size_t index)
{
    int16_t raw = (int16_t) ((uint16_t) bark_pcm[index * 2] |
                             ((uint16_t) bark_pcm[index * 2 + 1] << 8));
    return (int16_t) ((int32_t) raw * SPEAKER_MAX_VOLUME_PERCENT / 100);
}

static void load_bark(const char *path)
{
    FILE *file = NULL;
    assert(fopen_s(&file, path, "rb") == 0);
    uint8_t header[44];
    assert(fread(header, 1, sizeof(header), file) == sizeof(header));
    assert(memcmp(header, "RIFF", 4) == 0 && memcmp(header + 8, "WAVEfmt ", 8) == 0);
    assert(wav_read_u32(header + 4) == sizeof(bark_pcm) + 36);
    assert(wav_read_u32(header + 16) == 16);
    assert(header[20] == 1 && header[21] == 0 && header[22] == 1 && header[23] == 0);
    assert(wav_read_u32(header + 24) == SPEAKER_SAMPLE_RATE_HZ);
    assert(wav_read_u32(header + 28) == SPEAKER_SAMPLE_RATE_HZ * 2);
    assert(header[32] == 2 && header[33] == 0 && header[34] == 16 && header[35] == 0);
    assert(memcmp(header + 36, "data", 4) == 0);
    assert(wav_read_u32(header + 40) == sizeof(bark_pcm));
    assert(fread(bark_pcm, 1, sizeof(bark_pcm), file) == sizeof(bark_pcm));
    assert(fgetc(file) == EOF);
    assert(fclose(file) == 0);
}

static void test_bark(void)
{
    assert(strstr(tokki_action_find("speaker.dog_bark")->display_name, "recording") != NULL);
    assert(memcmp(bark_pcm, bark_pcm + sizeof(bark_pcm) / 2, sizeof(bark_pcm) / 2) == 0);
    unsigned nonzero = 0;
    for (size_t index = 0; index < sizeof(bark_pcm) / 2; ++index) {
        int16_t sample = bark_sample(index);
        assert(sample >= -6553 && sample <= 6553);
        nonzero += sample != 0;
    }
    assert(nonzero > 0);
    puts("PASS: recorded bark PCM16/16kHz/mono, exactly two identical repeats, volume ceiling");
}

static void wav_u16(FILE *file, uint16_t value)
{
    assert(fputc(value & 255, file) != EOF);
    assert(fputc(value >> 8, file) != EOF);
}

static void wav_u32(FILE *file, uint32_t value)
{
    wav_u16(file, (uint16_t) value);
    wav_u16(file, (uint16_t) (value >> 16));
}

static void export_bark_wav(const char *path)
{
    uint32_t data_size = (uint32_t) sizeof(bark_pcm) + SPEAKER_SAMPLE_RATE_HZ;
    FILE *file = NULL;
    assert(fopen_s(&file, path, "wb") == 0);
    assert(fwrite("RIFF", 1, 4, file) == 4);
    wav_u32(file, data_size + 36);
    assert(fwrite("WAVEfmt ", 1, 8, file) == 8);
    wav_u32(file, 16);
    wav_u16(file, 1);
    wav_u16(file, 1);
    wav_u32(file, SPEAKER_SAMPLE_RATE_HZ);
    wav_u32(file, SPEAKER_SAMPLE_RATE_HZ * 2);
    wav_u16(file, 2);
    wav_u16(file, 16);
    assert(fwrite("data", 1, 4, file) == 4);
    wav_u32(file, data_size);
    for (unsigned frame = 0; frame < SPEAKER_SAMPLE_RATE_HZ / 4; ++frame) {
        wav_u16(file, 0);
    }
    for (size_t index = 0; index < sizeof(bark_pcm) / 2; ++index) {
        wav_u16(file, (uint16_t) bark_sample(index));
    }
    for (unsigned frame = 0; frame < SPEAKER_SAMPLE_RATE_HZ / 4; ++frame) {
        wav_u16(file, 0);
    }
    assert(ftell(file) == (long) data_size + 44);
    assert(fclose(file) == 0);
    printf("PASS: exported recorded bark with firmware volume and padding: %s\n", path);
}

int main(int argc, char **argv)
{
    assert(argc >= 2);
    load_bark(argv[1]);
    if (argc == 4 && strcmp(argv[2], "--bark-wav") == 0) {
        test_bark();
        export_bark_wav(argv[3]);
        return 0;
    }
    if (argc == 3) {
        assert(fopen_s(&preview_output, argv[2], "w") == 0);
        fputs("window.TOKKI_PREVIEW = {\n", preview_output);
    }
    assert(tokki_action_count() == 47);
    assert(PET_EYES_HAPPY == 0 && PET_EYES_SLEEPY == 9);
    assert(PET_EYES_LOVEY_DOVEY == 10 && PET_EYES_SHY == 11);
    assert(PET_EYES_SLEEPING == 12);
    assert(tokki_action_at(30) == tokki_action_find("speaker.dog_bark"));
    assert(tokki_action_at(42) == tokki_action_find("oled.lovey_dovey"));
    assert(tokki_action_at(43) == tokki_action_find("oled.shy"));
    assert(tokki_action_at(44) == tokki_action_find("oled.night_sky"));
    assert(tokki_action_at(45) == tokki_action_find("oled.sunrise"));
    assert(tokki_action_at(46) == tokki_action_find("oled.sleeping"));
    assert(TOKKI_OLED_ART_EXCLAMATION == 6 && TOKKI_OLED_ART_NIGHT_SKY == 7 && TOKKI_OLED_ART_SUNRISE == 8);
    assert(tokki_action_at(tokki_action_count()) == NULL);
    assert(tokki_action_find(NULL) == NULL);
    assert(tokki_action_run("missing") == ESP_ERR_NOT_FOUND);
    assert(tokki_action_run(NULL) == ESP_ERR_NOT_FOUND);
    for (size_t index = 0; index < tokki_action_count(); ++index) {
        const tokki_action_descriptor_t *action = tokki_action_at(index);
        assert(action != NULL && action->run != NULL);
        assert(action->display_name != NULL && !action->cancellable);
        assert(tokki_action_find(action->id) == action);
        const char *device = tokki_device_name(action->device);
        assert(strncmp(action->id, device, strlen(device)) == 0);
        assert(action->id[strlen(device)] == '.');
        for (size_t other = index + 1; other < tokki_action_count(); ++other) {
            assert(strcmp(action->id, tokki_action_at(other)->id) != 0);
        }
    }
    reset_output();
    assert(tokki_action_run("led.blink") == ESP_OK);
    assert(elapsed_ms == 1800);
    test_blink("neopixel.blink_red", 32, 0);
    test_blink("neopixel.blink_yellow", 32, 32);
    test_blink("neopixel.blink_green", 0, 32);
    test_fade("neopixel.breathe_teal", 16, true);
    test_fade("neopixel.pulse_blue", 8, false);
    test_rainbow();
    test_oled("oled.happy", 48, 2880);
    test_oled("oled.sad", 48, 2880);
    test_oled("oled.surprised", 48, 2880);
    test_oled("oled.curious", 48, 2880);
    test_oled("oled.blink", 9, 540);
    test_oled("oled.drink_water", 1, 2000);
    test_oled("oled.water_drop", 24, 1440);
    test_oled("oled.fire", 24, 1440);
    test_oled("oled.wink", 10, 600);
    test_oled("oled.checkmark", 17, 1020);
    test_oled("oled.thinking", 20, 1200);
    test_oled("oled.look_left", 24, 1440);
    test_oled("oled.look_right", 24, 1440);
    test_oled("oled.look_up", 24, 1440);
    test_oled("oled.look_down", 24, 1440);
    test_oled("oled.sleepy", 24, 1440);
    test_oled("oled.heart", 24, 1440);
    test_oled("oled.exclamation", 17, 1020);
    test_oled("oled.lovey_dovey", 48, 2880);
    test_oled("oled.shy", 48, 2880);
    test_oled("oled.night_sky", 48, 2880);
    test_oled("oled.sunrise", 64, 3840);
    test_oled("oled.sleeping", 48, 2880);
    test_renderers();
    test_cartoon_eyes();
    test_affectionate_eyes();
    test_sleeping_eyes();
    test_glances();
    test_restoring_art();
    test_sky_art();
    test_speaker();
    test_bark();
    if (preview_output != NULL) {
        begin_preview("led.blink", 1800, "status");
        fputc('}', preview_output);
        begin_preview("neopixel.rainbow", 3840, "rainbow");
        fputc('}', preview_output);
        begin_preview("speaker.drink_water", 2230, "speech");
        fputc('}', preview_output);
        begin_preview("speaker.dog_bark", 1020, "recording");
        fprintf(preview_output, ",\"sampleRate\":%u,\"pcm\":\"", SPEAKER_SAMPLE_RATE_HZ);
        for (unsigned frame = 0; frame < SPEAKER_SAMPLE_RATE_HZ / 4; ++frame) {
            fputs("0000", preview_output);
        }
        for (size_t index = 0; index < sizeof(bark_pcm) / 2; ++index) {
            uint16_t sample = (uint16_t) bark_sample(index);
            fprintf(preview_output, "%02x%02x", (unsigned) (sample & 255), (unsigned) (sample >> 8));
        }
        for (unsigned frame = 0; frame < SPEAKER_SAMPLE_RATE_HZ / 4; ++frame) {
            fputs("0000", preview_output);
        }
        fputs("\"}", preview_output);
        assert(preview_actions == tokki_action_count());
        fputs("\n};\n", preview_output);
        assert(fclose(preview_output) == 0);
    }
    printf("PASS: %zu actions, RGB timing/colors, OLED frames/bounds/timing, speaker routing/envelope/ceiling, driver failures\n", tokki_action_count());
    return 0;
}