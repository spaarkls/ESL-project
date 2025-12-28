#include "color.h"

#include <math.h>
#include "defines.h"


void hsv_to_rgb(const struct HSV *hsv, struct RGB *rgb) {
    float h = hsv->hue;
    float s = hsv->saturation / 100.0f;
    float v = hsv->brightness / 100.0f;

    float c = v * s;
    float x = c * (1 - fabs(((int)h / 60) % 2 - 1));
    float m = v - c;

    float r1, g1, b1;
    if (h < 60) {
        r1 = c; g1 = x; b1 = 0;
    
    } else if (h < 120) {
        r1 = x; g1 = c; b1 = 0;
    
    } else if (h < 180) {
        r1 = 0; g1 = c; b1 = x; 
    
    } else if (h < 240) {
        r1 = 0; g1 = x; b1 = c;
    
    } else if (h < 300) {
        r1 = x; g1 = 0; b1 = c;

    } else {
        r1 = c; g1 = 0; b1 = x;

    }

    rgb->r = (uint16_t)((r1 + m) * PWM_TOP_VALUE);
    rgb->g = (uint16_t)((g1 + m) * PWM_TOP_VALUE);
    rgb->b = (uint16_t)((b1 + m) * PWM_TOP_VALUE);
}
