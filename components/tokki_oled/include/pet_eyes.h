#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum {
    PET_EYES_HAPPY,
    PET_EYES_SAD,
    PET_EYES_CURIOUS,
    PET_EYES_SURPRISED,
} pet_eye_expression_t;

void pet_eyes_render(uint8_t *framebuffer,
                     size_t framebuffer_size,
                     int width,
                     int height,
                     pet_eye_expression_t expression,
                     unsigned int frame);
