#ifndef COLOR_H__
#define COLOR_H__

#include <stdint.h>


#define HSV_DEFAULT_HUE         ( 353 )
#define HSV_DEFAULT_SATURATION  ( 100 )
#define HSV_DEFAULT_BRIGHTNESS  ( 100 )

#define RGB_DEFAULT_RED     ( 100 )
#define RGB_DEFAULT_GREEN   ( 100 )
#define RGB_DEFAULT_BLUE    ( 100 )


#define HSV_DEFAULT_CONFIG { \
    .hue = HSV_DEFAULT_HUE, \
    .saturation = HSV_DEFAULT_SATURATION, \
    .brightness = HSV_DEFAULT_BRIGHTNESS, \
}

#define RGB_DEFAULT_CONFIG { \
    .red = RGB_DEFAULT_RED, \
    .green = RGB_DEFAULT_GREEN, \
    .blue = RGB_DEFAULT_BLUE, \
}

struct HSV {
    int16_t hue;
    int16_t saturation;
    int16_t brightness;
};

struct RGB {
    uint16_t red;
    uint16_t green;
    uint16_t blue;
};

void color_hsv_to_rgb(const struct HSV *hsv, struct RGB *rgb);

void color_init_hsv(struct HSV *hsv);

void color_init_rgb(struct RGB *rgb);


#endif // COLOR_H__