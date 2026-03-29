#ifndef COLOR_H__
#define COLOR_H__

#include <stdint.h>


#define HSV_DEFAULT_HUE 353
#define HSV_DEFAULT_SATURATION 100
#define HSV_DEFAULT_BRIGHTNESS 100

#define HSV_DEFAULT_CONFIG { \
    .hue = HSV_DEFAULT_HUE, \
    .saturation = HSV_DEFAULT_SATURATION, \
    .brightness = HSV_DEFAULT_BRIGHTNESS, \
}

#define RGB_DEFAULT_CONFIG { \
    .r = 100, \
    .g = 100, \
    .b = 100, \
}

struct HSV {
    int16_t hue;
    int16_t saturation;
    int16_t brightness;
};

struct RGB {
    uint16_t r;
    uint16_t g;
    uint16_t b;
};

void hsv_to_rgb(const struct HSV *hsv, struct RGB *rgb);


#endif // COLOR_H__