#include "pet_eyes.h"

#include <stdbool.h>
#include <string.h>

typedef struct {
    int center_x;
    int center_y;
    int radius_x;
    int radius_y;
    int pupil_x;
    int pupil_y;
    int pupil_radius_x;
    int pupil_radius_y;
    int closure;
    int upper_lid;
    int lid_slant;
    bool heart_pupil;
} eye_geometry_t;

typedef struct {
    unsigned int frame;
    int value;
} motion_key_t;

typedef struct {
    uint8_t *pixels;
    size_t size;
    int width;
    int height;
} canvas_t;

static int clamp_int(int value, int minimum, int maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static void set_pixel(canvas_t *canvas, int x, int y, bool on)
{
    if (x < 0 || x >= canvas->width || y < 0 || y >= canvas->height) {
        return;
    }

    size_t index = (size_t) canvas->width * (y / 8) + x;
    if (index >= canvas->size) {
        return;
    }

    uint8_t mask = 1U << (y % 8);
    if (on) {
        canvas->pixels[index] |= mask;
    } else {
        canvas->pixels[index] &= (uint8_t) ~mask;
    }
}

static void fill_ellipse(canvas_t *canvas,
                         int center_x,
                         int center_y,
                         int radius_x,
                         int radius_y,
                         bool on)
{
    if (radius_x <= 0 || radius_y <= 0) {
        return;
    }

    int64_t radius_x_squared = (int64_t) radius_x * radius_x;
    int64_t radius_y_squared = (int64_t) radius_y * radius_y;
    int64_t ellipse_limit = radius_x_squared * radius_y_squared;

    for (int y = -radius_y; y <= radius_y; ++y) {
        for (int x = -radius_x; x <= radius_x; ++x) {
            int64_t distance = (int64_t) x * x * radius_y_squared +
                               (int64_t) y * y * radius_x_squared;
            if (distance <= ellipse_limit) {
                set_pixel(canvas, center_x + x, center_y + y, on);
            }
        }
    }
}

static void draw_line(canvas_t *canvas,
                      int x0,
                      int y0,
                      int x1,
                      int y1,
                      bool on)
{
    int delta_x = x1 > x0 ? x1 - x0 : x0 - x1;
    int step_x = x0 < x1 ? 1 : -1;
    int delta_y = y1 > y0 ? y0 - y1 : y1 - y0;
    int step_y = y0 < y1 ? 1 : -1;
    int error = delta_x + delta_y;

    while (true) {
        set_pixel(canvas, x0, y0, on);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int doubled_error = 2 * error;
        if (doubled_error >= delta_y) {
            error += delta_y;
            x0 += step_x;
        }
        if (doubled_error <= delta_x) {
            error += delta_x;
            y0 += step_y;
        }
    }
}

static void draw_quadratic_curve(canvas_t *canvas,
                                 int x0,
                                 int y0,
                                 int control_x,
                                 int control_y,
                                 int x1,
                                 int y1,
                                 int thickness)
{
    const int steps = 20;
    int previous_x = x0;
    int previous_y = y0;
    int half_thickness = thickness / 2;

    for (int step = 1; step <= steps; ++step) {
        int inverse = steps - step;
        int denominator = steps * steps;
        int x = (
            inverse * inverse * x0 +
            2 * inverse * step * control_x +
            step * step * x1
        ) / denominator;
        int y = (
            inverse * inverse * y0 +
            2 * inverse * step * control_y +
            step * step * y1
        ) / denominator;

        for (int offset = -half_thickness;
             offset <= half_thickness;
             ++offset) {
            draw_line(canvas,
                      previous_x,
                      previous_y + offset,
                      x,
                      y + offset,
                      true);
        }
        previous_x = x;
        previous_y = y;
    }

    fill_ellipse(canvas, x0, y0, half_thickness, half_thickness, true);
    fill_ellipse(canvas, x1, y1, half_thickness, half_thickness, true);
}

static int interpolate(int from, int to, int percent)
{
    return from + (to - from) * percent / 100;
}

static int sample_motion(const motion_key_t *keys, size_t count, unsigned int frame)
{
    for (size_t i = 1; i < count; ++i) {
        if (frame <= keys[i].frame) {
            int phase = (int) ((frame - keys[i - 1].frame) * 100 /
                              (keys[i].frame - keys[i - 1].frame));
            int eased = phase * phase * (300 - 2 * phase) / 10000;
            return interpolate(keys[i - 1].value, keys[i].value, eased);
        }
    }
    return keys[count - 1].value;
}

static int blink_closure(unsigned int frame)
{
    /* oled.blink and oled.wink enter directly at frame 34. */
    static const int closure[] = {0, 25, 55, 85, 100, 85, 55, 25, 0};
    return frame >= 34 && frame <= 42 ? closure[frame - 34] : 0;
}

static int animate_pupil_overshoot(unsigned int frame)
{
    static const motion_key_t keys[] = {
        {0, 0}, {4, 125}, {7, 100}, {15, 100},
        {19, -12}, {22, 0}, {23, 0},
    };
    return sample_motion(keys, sizeof(keys) / sizeof(keys[0]), frame);
}

static void animate_squash_stretch(eye_geometry_t *eye, int closure)
{
    eye->closure = closure;
    eye->radius_x += 3 * closure / 100;
    eye->radius_y = interpolate(eye->radius_y, 7, closure);
    eye->center_y -= 2 * closure / 100;
    eye->pupil_y = eye->center_y + 5 * closure / 100;
    eye->pupil_radius_y = interpolate(eye->pupil_radius_y, 3, closure);
}

static void draw_closed_crescent_eye(canvas_t *canvas, const eye_geometry_t *eye)
{
    draw_quadratic_curve(canvas,
                         eye->center_x - eye->radius_x, eye->center_y + 3,
                         eye->center_x, eye->center_y - 13,
                         eye->center_x + eye->radius_x, eye->center_y + 3,
                         3);
}

static void apply_lid_masks(canvas_t *canvas, const eye_geometry_t *eye)
{
    for (int x = -eye->radius_x; x <= eye->radius_x; ++x) {
        int upper = -eye->radius_y + eye->upper_lid;
        if (eye->upper_lid > 0) {
            upper += eye->lid_slant * x / eye->radius_x +
                     2 * x * x / (eye->radius_x * eye->radius_x);
        }
        /* The lower lid rises into an arch as the oval squashes to a smile. */
        int lower = eye->radius_y -
                    (eye->radius_y + 4) * eye->closure / 100 +
                    8 * eye->closure * x * x /
                    (100 * eye->radius_x * eye->radius_x);
        for (int y = -eye->radius_y; y <= eye->radius_y; ++y) {
            if (y < upper || y > lower) {
                set_pixel(canvas, eye->center_x + x, eye->center_y + y, false);
            }
        }
    }
}

static void draw_heart_pupil(canvas_t *canvas, const eye_geometry_t *eye)
{
    int lobe_radius = eye->pupil_radius_x / 2;
    int lobe_offset = eye->pupil_radius_x - lobe_radius;
    int top = -eye->pupil_radius_y / 3;
    fill_ellipse(canvas, eye->pupil_x - lobe_offset, eye->pupil_y + top,
                 lobe_radius, lobe_radius, false);
    fill_ellipse(canvas, eye->pupil_x + lobe_offset, eye->pupil_y + top,
                 lobe_radius, lobe_radius, false);
    for (int y = top; y <= eye->pupil_radius_y; ++y) {
        int half_width = (eye->pupil_radius_y - y) * eye->pupil_radius_x /
                         (eye->pupil_radius_y - top);
        draw_line(canvas, eye->pupil_x - half_width, eye->pupil_y + y,
                  eye->pupil_x + half_width, eye->pupil_y + y, false);
    }
}

static void draw_open_eye(canvas_t *canvas, const eye_geometry_t *eye)
{
    fill_ellipse(canvas, eye->center_x, eye->center_y,
                 eye->radius_x, eye->radius_y, true);
    if (eye->heart_pupil) {
        draw_heart_pupil(canvas, eye);
    } else {
        fill_ellipse(canvas, eye->pupil_x, eye->pupil_y,
                     eye->pupil_radius_x, eye->pupil_radius_y, false);

        /* Pin pupils stay solid; catchlights are clipped by the lids with the pupil. */
        if (eye->pupil_radius_x >= 5 && eye->pupil_radius_y >= 7) {
            fill_ellipse(canvas, eye->pupil_x - 2, eye->pupil_y - 3, 1, 1, true);
        }
    }
    apply_lid_masks(canvas, eye);
}

static void configure_expression_geometry(eye_geometry_t eyes[2],
                                          pet_eye_expression_t expression,
                                          unsigned int frame)
{
    unsigned int position = frame % 48;
    unsigned int short_position = frame % 24;
    for (int i = 0; i < 2; ++i) {
        eyes[i] = (eye_geometry_t) {
            .center_x = 40 + i * 48,
            .center_y = 32,
            .radius_x = 17,
            .radius_y = 22,
            .pupil_x = 40 + i * 48,
            .pupil_y = 32,
            .pupil_radius_x = 6,
            .pupil_radius_y = 10,
        };
    }

    switch (expression) {
    case PET_EYES_HAPPY: {
        static const motion_key_t smile[] = {
            {0, 0}, {2, 0}, {7, 100}, {22, 100}, {29, 0}, {33, 0},
        };
        int closure = position < 34 ?
            sample_motion(smile, sizeof(smile) / sizeof(smile[0]), position) :
            blink_closure(position);
        for (int i = 0; i < 2; ++i) {
            animate_squash_stretch(&eyes[i], closure);
        }
        break;
    }
    case PET_EYES_SAD:
        for (int i = 0; i < 2; ++i) {
            eyes[i].radius_y = 20;
            eyes[i].upper_lid = 9;
            eyes[i].lid_slant = i == 0 ? -4 : 4;
            eyes[i].pupil_x += i == 0 ? 2 : -2;
            eyes[i].pupil_y += 5;
        }
        break;
    case PET_EYES_CURIOUS: {
        static const motion_key_t drift[] = {
            {0, 0}, {6, 5}, {10, 4}, {19, 4}, {27, -4},
            {32, -3}, {39, -3}, {47, 0},
        };
        static const motion_key_t stretch[] = {
            {0, 0}, {5, 100}, {10, 0}, {26, 0},
            {31, 75}, {36, 0}, {47, 0},
        };
        int spring = sample_motion(stretch, sizeof(stretch) / sizeof(stretch[0]), position);
        eyes[0].radius_x = 18 - spring / 100;
        eyes[0].radius_y = 24 + 2 * spring / 100;
        eyes[0].pupil_x += sample_motion(drift, sizeof(drift) / sizeof(drift[0]), position);
        eyes[0].pupil_y -= 2;
        eyes[1].radius_x = 15 + spring / 100;
        eyes[1].radius_y = 18 - 2 * spring / 100;
        eyes[1].center_y += 1;
        eyes[1].upper_lid = 3;
        eyes[1].pupil_radius_x = 5;
        eyes[1].pupil_radius_y = 8;
        eyes[1].pupil_x += sample_motion(drift, sizeof(drift) / sizeof(drift[0]),
                                          position > 2 ? position - 2 : 0);
        break;
    }
    case PET_EYES_SURPRISED: {
        static const motion_key_t pop[] = {
            {0, 0}, {2, 75}, {4, 125}, {7, 100}, {47, 100},
        };
        int amount = sample_motion(pop, sizeof(pop) / sizeof(pop[0]), position);
        int shrink = clamp_int(amount, 0, 100);
        for (int i = 0; i < 2; ++i) {
            eyes[i].radius_x += amount / 100;
            eyes[i].radius_y += 5 * amount / 100;
            eyes[i].pupil_radius_x = interpolate(6, 2, shrink);
            eyes[i].pupil_radius_y = interpolate(10, 2, shrink);
        }
        break;
    }
    case PET_EYES_WINK:
        animate_squash_stretch(&eyes[1], blink_closure(position));
        break;
    case PET_EYES_LOOK_LEFT:
    case PET_EYES_LOOK_RIGHT:
    case PET_EYES_LOOK_UP:
    case PET_EYES_LOOK_DOWN: {
        int amount = animate_pupil_overshoot(short_position);
        for (int i = 0; i < 2; ++i) {
            if (expression == PET_EYES_LOOK_LEFT || expression == PET_EYES_LOOK_RIGHT) {
                eyes[i].pupil_x += (expression == PET_EYES_LOOK_LEFT ? -8 : 8) * amount / 100;
            } else {
                eyes[i].pupil_y += (expression == PET_EYES_LOOK_UP ? -6 : 6) * amount / 100;
            }
        }
        break;
    }
    case PET_EYES_SLEEPY: {
        static const motion_key_t droop[] = {
            {0, 0}, {5, 50}, {13, 100}, {17, 100}, {22, 0}, {23, 0},
        };
        int amount = sample_motion(droop, sizeof(droop) / sizeof(droop[0]), short_position);
        for (int i = 0; i < 2; ++i) {
            eyes[i].radius_y -= 3 * amount / 100;
            eyes[i].upper_lid = 26 * amount / 100;
            eyes[i].lid_slant = (i == 0 ? 2 : -2) * amount / 100;
            eyes[i].pupil_y += 4 * amount / 100;
        }
        break;
    }
    case PET_EYES_LOVEY_DOVEY: {
        static const motion_key_t heartbeat[] = {
            {0, 0}, {6, 100}, {12, 0}, {20, 0}, {26, 100}, {32, 0}, {47, 0},
        };
        static const motion_key_t smile[] = {
            {0, 0}, {33, 0}, {38, 100}, {41, 100}, {46, 0}, {47, 0},
        };
        int pulse = sample_motion(heartbeat, sizeof(heartbeat) / sizeof(heartbeat[0]), position);
        int closure = sample_motion(smile, sizeof(smile) / sizeof(smile[0]), position);
        for (int i = 0; i < 2; ++i) {
            eyes[i].heart_pupil = position < 38;
            if (eyes[i].heart_pupil) {
                eyes[i].radius_x += pulse / 100;
                eyes[i].radius_y += 2 * pulse / 100;
                eyes[i].pupil_radius_x = 8 + 2 * pulse / 100;
                eyes[i].pupil_radius_y = 8 + 2 * pulse / 100;
            }
            animate_squash_stretch(&eyes[i], closure);
        }
        break;
    }
    case PET_EYES_SHY: {
        static const motion_key_t retreat[] = {
            {0, 0}, {8, 100}, {16, 100}, {23, 35}, {28, 35},
            {34, 85}, {39, 85}, {46, 0}, {47, 0},
        };
        int amount = sample_motion(retreat, sizeof(retreat) / sizeof(retreat[0]), position);
        for (int i = 0; i < 2; ++i) {
            eyes[i].center_y += 2 * amount / 100;
            eyes[i].radius_y -= 4 * amount / 100;
            eyes[i].upper_lid = (10 + i * 2) * amount / 100;
            eyes[i].closure = 20 * amount / 100;
            eyes[i].pupil_x += (i == 0 ? 5 : -5) * amount / 100;
            eyes[i].pupil_y = eyes[i].center_y + 6 * amount / 100;
            eyes[i].pupil_radius_y -= amount / 100;
        }
        break;
    }
    }
}

void pet_eyes_render(uint8_t *framebuffer,
                     size_t framebuffer_size,
                     int width,
                     int height,
                     pet_eye_expression_t expression,
                     unsigned int frame)
{
    if (framebuffer == NULL ||
        framebuffer_size < (size_t) width * height / 8 ||
        width <= 0 ||
        height <= 0) {
        return;
    }

    memset(framebuffer, 0, framebuffer_size);
    canvas_t canvas = {
        .pixels = framebuffer,
        .size = framebuffer_size,
        .width = width,
        .height = height,
    };

    eye_geometry_t eyes[2];
    configure_expression_geometry(eyes, expression, frame);

    for (size_t i = 0; i < sizeof(eyes) / sizeof(eyes[0]); ++i) {
        if (eyes[i].closure == 100) {
            draw_closed_crescent_eye(&canvas, &eyes[i]);
        } else {
            draw_open_eye(&canvas, &eyes[i]);
        }
    }
}
