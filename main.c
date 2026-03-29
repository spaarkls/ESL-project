#include <stdbool.h>
#include <stdint.h>
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrfx_gpiote.h"
#include "nrfx_clock.h"
#include "nrfx_pwm.h"
#include "app_timer.h"

#include "color.h"
#include "defines.h"


#define BUTTON NRF_GPIO_PIN_MAP(1, 6)

#define LED_Y NRF_GPIO_PIN_MAP(0, 6)
#define LED_R NRF_GPIO_PIN_MAP(0, 8)
#define LED_G NRF_GPIO_PIN_MAP(1, 9)
#define LED_B NRF_GPIO_PIN_MAP(0, 12)


typedef enum {
    STATE_COLOR_NONE = 0,
    STATE_COLOR_SELECT_HUE,
    STATE_COLOR_SELECT_SATURATION, 
    STATE_COLOR_SELECT_BRIGHTNESS,
    STATE_COLOR_MAX,
} STATE_COLOR;


struct app_color {
    struct HSV hsv;
    struct RGB rgb;
    STATE_COLOR current_state;
    uint16_t state_value;
    bool fade_up_yellow;
    bool fade_up_other;
};

APP_TIMER_DEF(timer_double_click);
APP_TIMER_DEF(timer_debouncing);
APP_TIMER_DEF(timer_update_color);

static struct app_color *g_app = NULL;

static nrfx_pwm_t pwm0 = NRFX_PWM_INSTANCE(0);
static nrf_pwm_values_individual_t pwm_vals;
static nrf_pwm_sequence_t pwm_seq;


static volatile bool button_block = false;
static volatile bool button_first_click = false;
static volatile bool button_is_pressed = false;

static void init_app(struct app_color *app);

static void init_gpiote(void *ctx);
static void init_timers(void *ctx);
static void init_pwm(void *ctx);

static void handler_button_pressed(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action);
static void handler_debouncing_timer(void *ctx);
static void handler_double_click_timer(void *ctx);
static void process_led_1(void *ctx);


int main(void) {
    struct app_color app;
    init_app(&app);
    g_app = &app;

    init_timers(&app);
    init_gpiote(&app);
    init_pwm(&app);

    while (true) {
        __WFE();
    }

    return 0;
}

static void handler_debouncing_timer(void *ctx) {
    (void)ctx;
    button_block = false;
}


static void handler_double_click_timer(void *ctx) {
    (void)ctx;
    button_first_click = false;
}


static void handler_update_color_timer(void *ctx) {
    VALID_PTR(ctx);
    struct app_color *app = ctx;
    process_led_1(app);

    if (button_is_pressed) {
        if (nrf_gpio_pin_read(BUTTON) != 0) {
            button_is_pressed = false;
        }
    }

    if (!button_is_pressed) {
        return;
    }

    if (app->current_state == STATE_COLOR_NONE) {
        return;
    }

    switch (app->current_state) {
        case STATE_COLOR_SELECT_HUE:
            if (app->fade_up_other) {
                app->hsv.hue += STEP_CHANGE_HUE;
                if (app->hsv.hue >= MAX_VALUE_HUE) {
                    app->hsv.hue = MAX_VALUE_HUE;
                    app->fade_up_other = false;
                }

            } else {
                app->hsv.hue -= STEP_CHANGE_HUE;
                if (app->hsv.hue < STEP_CHANGE_HUE) {
                    app->hsv.hue = 0;
                    app->fade_up_other = true;
                }
            }
            break;

        case STATE_COLOR_SELECT_SATURATION:
            if (app->fade_up_other) {
                app->hsv.saturation += STEP_CHANGE_SATURATION;
                if (app->hsv.saturation >= MAX_VALUE_SATURATION) {
                    app->hsv.saturation = MAX_VALUE_SATURATION;
                    app->fade_up_other = false;
                }
            } else {
                app->hsv.saturation -= STEP_CHANGE_SATURATION;
                if (app->hsv.saturation < STEP_CHANGE_SATURATION) {
                    app->hsv.saturation = 0;
                    app->fade_up_other = true;
                }
            }
            break;

        case STATE_COLOR_SELECT_BRIGHTNESS:
            if (app->fade_up_other) {
                app->hsv.brightness += STEP_CHANGE_BRIGHT;
                if (app->hsv.brightness >= MAX_VALUE_BRIGHTNESS) {
                    app->hsv.brightness = MAX_VALUE_BRIGHTNESS;
                    app->fade_up_other = false;
                }
            } else {
                app->hsv.brightness -= STEP_CHANGE_BRIGHT;
                if (app->hsv.brightness < STEP_CHANGE_BRIGHT) {
                    app->hsv.brightness = 0;
                    app->fade_up_other = true;
                }
            }
            break;

        default:
            break;
    }

    color_hsv_to_rgb(&app->hsv, &app->rgb);
    pwm_vals.channel_1 = app->rgb.red;
    pwm_vals.channel_2 = app->rgb.green;
    pwm_vals.channel_3 = app->rgb.blue;

    nrf_pwm_sequence_t seq = {
        .values.p_individual = &pwm_vals,
        .length = 4,
        .repeats = 0,
        .end_delay = 0
    };
    nrfx_pwm_simple_playback(&pwm0, &seq, 1, 0);
}


static void process_led_1(void *ctx) {
    VALID_PTR(ctx);
    struct app_color *app = ctx;
    
    switch (app->current_state) {
        case STATE_COLOR_NONE:
            app->state_value = 0;
            break;

        case STATE_COLOR_SELECT_HUE:
            if (app->fade_up_yellow) {
                app->state_value += STEP_CHANGE_YELLOW_COLOR_SLOW;
                if (app->state_value >= PWM_TOP_VALUE) {
                    app->state_value = PWM_TOP_VALUE;
                    app->fade_up_yellow = false;
                }

            } else {
                app->state_value -= STEP_CHANGE_YELLOW_COLOR_SLOW;
                if (app->state_value < STEP_CHANGE_YELLOW_COLOR_SLOW) {
                    app->state_value = 0;
                    app->fade_up_yellow = true;
                }
            }
            break;

        case STATE_COLOR_SELECT_SATURATION:
            if (app->fade_up_yellow) {
                app->state_value += STEP_CHANGE_YELLOW_COLOR_FAST;
                if (app->state_value >= PWM_TOP_VALUE) {
                    app->state_value = PWM_TOP_VALUE;
                    app->fade_up_yellow = false;
                }

            } else {
                app->state_value -= STEP_CHANGE_YELLOW_COLOR_FAST;
                if (app->state_value < STEP_CHANGE_YELLOW_COLOR_FAST) {
                    app->state_value = 0;
                    app->fade_up_yellow = true;
                }
            }
            break;

        case STATE_COLOR_SELECT_BRIGHTNESS:
            app->state_value = PWM_TOP_VALUE;
            break;

        default:
            break;
    }
    
    pwm_vals.channel_0 = app->state_value;

    nrf_pwm_sequence_t seq = {
        .values.p_individual = &pwm_vals,
        .length = 4,
        .repeats = 0,
        .end_delay = 0
    };
    nrfx_pwm_simple_playback(&pwm0, &seq, 1, 0);
}


static void init_app(struct app_color *app) {
    VALID_PTR(app);

    color_init_hsv(&app->hsv);
    color_init_rgb(&app->rgb);

    app->current_state = STATE_COLOR_NONE;
    app->fade_up_other = true;
    app->fade_up_yellow = true;
    app->state_value = 0;
}


static void init_timers(void *ctx) {
    VALID_PTR(ctx);
    struct app_color *app = ctx;

    nrfx_clock_init(NULL);
    nrfx_clock_lfclk_start();
    app_timer_init();
    app_timer_create(&timer_debouncing, APP_TIMER_MODE_SINGLE_SHOT, handler_debouncing_timer);
    app_timer_create(&timer_double_click, APP_TIMER_MODE_SINGLE_SHOT, handler_double_click_timer);
    app_timer_create(&timer_update_color, APP_TIMER_MODE_REPEATED, handler_update_color_timer);
    app_timer_start(timer_update_color, APP_TIMER_TICKS(DELAY_INTERVAL_FADE_MS), app);
}


static void init_pwm(void *ctx) {
    VALID_PTR(ctx);
    struct app_color *app = ctx;

    nrfx_pwm_config_t config_pwm = {
        .output_pins = {
            LED_Y,
            LED_R,
            LED_G,
            LED_B,
        }, 
        .base_clock = NRF_PWM_CLK_1MHz,
        .count_mode = NRF_PWM_MODE_UP,
        .top_value = PWM_TOP_VALUE,
        .load_mode = NRF_PWM_LOAD_INDIVIDUAL,
        .step_mode = NRF_PWM_STEP_AUTO
    };
    
    nrfx_pwm_init(&pwm0, &config_pwm, NULL);
    
    color_hsv_to_rgb(&app->hsv, &app->rgb);

    pwm_vals.channel_0 = 0;

    pwm_vals.channel_1 = app->rgb.red;
    pwm_vals.channel_2 = app->rgb.green;
    pwm_vals.channel_3 = app->rgb.blue;
}


static void handler_button_pressed(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action) {
    (void)action;
    
    if (pin == BUTTON) {
        
        if (button_block) {
            return;
        }
        button_block = true;
        app_timer_start(timer_debouncing, APP_TIMER_TICKS(DELAY_DEBOUNCE_MS), NULL);
        if (!button_first_click) {
            button_first_click = true;
            app_timer_start(timer_double_click, APP_TIMER_TICKS(DELAY_DOUBLE_CLICK_MS), NULL);
        
        } else {
            g_app->current_state = (g_app->current_state + 1) % STATE_COLOR_MAX;
            button_first_click = false;

            g_app->state_value = 0;
            g_app->fade_up_other = true;
            g_app->fade_up_yellow = true;

            app_timer_stop(timer_double_click);
        }
    }   
    button_is_pressed = true;
}


static void init_gpiote(void *ctx) {
    (void)ctx;

    if (!nrfx_gpiote_is_init()) {
        nrfx_gpiote_init();
    }

    nrfx_gpiote_in_config_t config_in = NRFX_GPIOTE_CONFIG_IN_SENSE_HITOLO(true);
    config_in.pull = NRF_GPIO_PIN_PULLUP;
    nrfx_gpiote_in_init(BUTTON, &config_in, handler_button_pressed);
    nrfx_gpiote_in_event_enable(BUTTON, true);
}