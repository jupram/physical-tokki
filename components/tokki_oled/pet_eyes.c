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
    int openness;
} eye_geometry_t;

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

static int blink_openness(unsigned int frame)
{
    static const int blink_curve[] = {100, 78, 46, 14, 6, 14, 46, 78};
    unsigned int position = frame % 48;
    if (position < 34 || position >= 42) {
        return 100;
    }
    return blink_curve[position - 34];
}

static int pupil_saccade(unsigned int frame)
{
    static const int positions[] = {0, 0, -2, -4, -4, -1, 0, 3, 4, 4, 1, 0};
    return positions[(frame / 3) % (
        sizeof(positions) / sizeof(positions[0])
    )];
}

static void draw_closed_lid(canvas_t *canvas,
                            const eye_geometry_t *eye,
                            bool happy)
{
    int half_width = eye->radius_x;
    int center_y = eye->center_y + (happy ? -2 : 2);
    int curve = happy ? -5 : 4;

    draw_line(canvas,
              eye->center_x - half_width,
              center_y,
              eye->center_x,
              center_y + curve,
              true);
    draw_line(canvas,
              eye->center_x,
              center_y + curve,
              eye->center_x + half_width,
              center_y,
              true);
    draw_line(canvas,
              eye->center_x - half_width,
              center_y + 1,
              eye->center_x,
              center_y + curve + 1,
              true);
    draw_line(canvas,
              eye->center_x,
              center_y + curve + 1,
              eye->center_x + half_width,
              center_y + 1,
              true);
}

static void draw_open_eye(canvas_t *canvas, const eye_geometry_t *eye)
{
    int visible_radius_y = eye->radius_y * eye->openness / 100;
    visible_radius_y = clamp_int(visible_radius_y, 1, eye->radius_y);

    fill_ellipse(canvas,
                 eye->center_x,
                 eye->center_y,
                 eye->radius_x,
                 visible_radius_y,
                 true);

    int pupil_y = clamp_int(
        eye->pupil_y,
        eye->center_y - visible_radius_y + eye->pupil_radius_y,
        eye->center_y + visible_radius_y - eye->pupil_radius_y
    );
    fill_ellipse(canvas,
                 eye->pupil_x,
                 pupil_y,
                 eye->pupil_radius_x,
                 eye->pupil_radius_y,
                 false);

    fill_ellipse(canvas,
                 eye->pupil_x - 2,
                 pupil_y - 3,
                 2,
                 2,
                 true);
    set_pixel(canvas, eye->pupil_x + 2, pupil_y + 2, true);
}

static void draw_eyebrows(canvas_t *canvas,
                          pet_eye_expression_t expression,
                          unsigned int frame)
{
    static const int motion[] = {0, -1, -1, 0, 1, 1, 0, 0};
    int vertical_motion = motion[(frame / 4) % (
        sizeof(motion) / sizeof(motion[0])
    )];

    switch (expression) {
    case PET_EYES_SURPRISED:
        draw_quadratic_curve(canvas, 24, 10, 40, 0, 56, 10, 3);
        draw_quadratic_curve(canvas, 72, 10, 88, 0, 104, 10, 3);
        break;
    case PET_EYES_HAPPY:
    case PET_EYES_WINK:
    case PET_EYES_LOOK_LEFT:
    case PET_EYES_LOOK_RIGHT:
    case PET_EYES_LOOK_UP:
    case PET_EYES_LOOK_DOWN:
    case PET_EYES_SLEEPY:
        draw_quadratic_curve(canvas,
                             22, 16 + vertical_motion,
                             39, 7 + vertical_motion,
                             57, 14 + vertical_motion,
                             3);
        draw_quadratic_curve(canvas,
                             71, 14 + vertical_motion,
                             89, 7 + vertical_motion,
                             106, 16 + vertical_motion,
                             3);
        break;
    case PET_EYES_SAD:
        draw_quadratic_curve(canvas,
                             22, 19,
                             39, 16 + vertical_motion,
                             57, 9 + vertical_motion,
                             3);
        draw_quadratic_curve(canvas,
                             71, 9 + vertical_motion,
                             89, 16 + vertical_motion,
                             106, 19,
                             3);
        break;
    case PET_EYES_CURIOUS:
        draw_quadratic_curve(canvas,
                             19, 13 + vertical_motion,
                             39, 1 + vertical_motion,
                             59, 10 + vertical_motion,
                             3);
        draw_quadratic_curve(canvas,
                             74, 20,
                             89, 16,
                             105, 20,
                             3);
        break;
    }
}

static void draw_teardrop(canvas_t *canvas, int center_x, int tip_y)
{
    set_pixel(canvas, center_x, tip_y, true);
    set_pixel(canvas, center_x - 1, tip_y + 1, true);
    set_pixel(canvas, center_x, tip_y + 1, true);
    set_pixel(canvas, center_x + 1, tip_y + 1, true);
    fill_ellipse(canvas, center_x, tip_y + 4, 3, 3, true);
}

static void configure_expression(eye_geometry_t eyes[2],
                                 pet_eye_expression_t expression,
                                 unsigned int frame)
{
    int movement = pupil_saccade(frame);
    int openness = blink_openness(frame);

    eyes[0] = (eye_geometry_t) {
        .center_x = 40,
        .center_y = 32,
        .radius_x = 17,
        .radius_y = 17,
        .pupil_x = 40 + movement,
        .pupil_y = 32,
        .pupil_radius_x = 6,
        .pupil_radius_y = 9,
        .openness = openness,
    };
    eyes[1] = (eye_geometry_t) {
        .center_x = 88,
        .center_y = 32,
        .radius_x = 17,
        .radius_y = 17,
        .pupil_x = 88 + movement,
        .pupil_y = 32,
        .pupil_radius_x = 6,
        .pupil_radius_y = 9,
        .openness = openness,
    };

    switch (expression) {
    case PET_EYES_SURPRISED:
        eyes[0].radius_y = 19;
        eyes[1].radius_y = 19;
        eyes[0].pupil_radius_x = 4;
        eyes[1].pupil_radius_x = 4;
        eyes[0].pupil_radius_y = 5;
        eyes[1].pupil_radius_y = 5;
        break;
    case PET_EYES_HAPPY:
    case PET_EYES_WINK:
    case PET_EYES_LOOK_LEFT:
    case PET_EYES_LOOK_RIGHT:
    case PET_EYES_LOOK_UP:
    case PET_EYES_LOOK_DOWN:
    case PET_EYES_SLEEPY:
        eyes[0].openness = openness * 72 / 100;
        eyes[1].openness = openness * 72 / 100;
        eyes[0].pupil_y = 30;
        eyes[1].pupil_y = 30;
        if (expression == PET_EYES_WINK) {
            eyes[0].openness = 72;
        }
        break;
    case PET_EYES_SAD:
        eyes[0].pupil_y = 36;
        eyes[1].pupil_y = 36;
        break;
    case PET_EYES_CURIOUS:
        eyes[0].radius_x = 19;
        eyes[0].radius_y = 19;
        eyes[0].pupil_x = 40 + movement;
        eyes[1].radius_x = 13;
        eyes[1].radius_y = 14;
        eyes[1].pupil_radius_x = 5;
        eyes[1].pupil_radius_y = 7;
        eyes[1].pupil_x = 88 - movement / 2;
        break;
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
    configure_expression(eyes, expression, frame);
    if (expression >= PET_EYES_LOOK_LEFT && expression <= PET_EYES_SLEEPY) {
        unsigned position = frame % 24;
        int amount = position < 5 ? (int) position :
                     position < 16 ? 4 : position < 20 ? 20 - (int) position : 0;
        for (size_t index = 0; index < 2; ++index) {
            eyes[index].pupil_x = eyes[index].center_x;
            eyes[index].pupil_y = 30;
            eyes[index].openness = 72;
            if (expression == PET_EYES_LOOK_LEFT || expression == PET_EYES_LOOK_RIGHT) {
                eyes[index].pupil_x += expression == PET_EYES_LOOK_LEFT ? -amount * 2 : amount * 2;
            } else if (expression == PET_EYES_LOOK_UP || expression == PET_EYES_LOOK_DOWN) {
                eyes[index].pupil_y += expression == PET_EYES_LOOK_UP ? -amount : amount;
            } else {
                eyes[index].openness = 72 - amount * 16;
            }
        }
    }
    draw_eyebrows(&canvas, expression, frame);

    for (size_t i = 0; i < sizeof(eyes) / sizeof(eyes[0]); ++i) {
        if (eyes[i].openness <= 20) {
            draw_closed_lid(&canvas, &eyes[i],
                            expression == PET_EYES_HAPPY || expression >= PET_EYES_WINK);
        } else {
            draw_open_eye(&canvas, &eyes[i]);
        }
    }

    if (expression == PET_EYES_SAD) {
        int left_phase = frame % 12;
        int right_phase = (frame + 6) % 12;
        draw_teardrop(&canvas, 53, 41 + left_phase * 2);
        draw_teardrop(&canvas, 75, 41 + right_phase * 2);
    }
}
