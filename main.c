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

APP_TIMER_DEF(timer_double_click);
APP_TIMER_DEF(timer_debouncing);
APP_TIMER_DEF(timer_update_color);

static volatile STATE_COLOR current_color_state = STATE_COLOR_NONE;
static struct HSV hsv = HSV_DEFAULT_CONFIG;
static struct RGB rgb = RGB_DEFAULT_CONFIG;

static nrfx_pwm_t pwm0 = NRFX_PWM_INSTANCE(0);
static nrf_pwm_values_individual_t pwm_vals;
static nrf_pwm_sequence_t pwm_seq;

static uint16_t value = 0;
static uint16_t value_hue = HSV_DEFAULT_HUE;
static uint16_t value_satur = HSV_DEFAULT_SATURATION;
static uint16_t value_bright = HSV_DEFAULT_BRIGHTNESS;

static volatile bool button_block = false;
static volatile bool button_first_click = false;
static volatile bool button_is_pressed = false;
static bool fade_up_yellow = true;
static bool fade_up_other = true;

static void init_gpiote(void);
static void init_timers(void);
static void init_pwm(void);
static void handler_button_pressed(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action);
static void handler_debouncing_timer(void *ctx);
static void handler_double_click_timer(void *ctx);
static void process_led_1(void);


int main(void) {
    init_timers();
    init_gpiote();
    init_pwm();

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
    (void)ctx;

    process_led_1();

    if (button_is_pressed) {
        if (nrf_gpio_pin_read(BUTTON) != 0) {
            button_is_pressed = false;
        }
    }

    if (!button_is_pressed) {
        return;
    }

    if (current_color_state == STATE_COLOR_NONE) {
        return;
    }

    switch (current_color_state) {
        case STATE_COLOR_SELECT_HUE:
            if (fade_up_other) {
                value_hue += STEP_CHANGE_HUE;
                if (value_hue >= MAX_VALUE_HUE) {
                    value_hue = MAX_VALUE_HUE;
                    fade_up_other = false;
                }
            } else {
                value_hue -= STEP_CHANGE_HUE;
                if (value_hue < STEP_CHANGE_HUE) {
                    value_hue = 0;
                    fade_up_other = true;
                }
            }
            break;

        case STATE_COLOR_SELECT_SATURATION:
            if (fade_up_other) {
                value_satur += STEP_CHANGE_SATURATION;
                if (value_satur >= MAX_VALUE_SATURATION) {
                    value_satur = MAX_VALUE_SATURATION;
                    fade_up_other = false;
                }
            } else {
                value_satur -= STEP_CHANGE_SATURATION;
                if (value_satur < STEP_CHANGE_SATURATION) {
                    value_satur = 0;
                    fade_up_other = true;
                }
            }
            break;

        case STATE_COLOR_SELECT_BRIGHTNESS:
            if (fade_up_other) {
                value_bright += STEP_CHANGE_BRIGHT;
                if (value_bright >= MAX_VALUE_BRIGHTNESS) {
                    value_bright = MAX_VALUE_BRIGHTNESS;
                    fade_up_other = false;
                }
            } else {
                value_bright -= STEP_CHANGE_BRIGHT;
                if (value_bright < STEP_CHANGE_BRIGHT) {
                    value_bright = 0;
                    fade_up_other = true;
                }
            }
            break;

        default:
            break;
    }

    hsv.hue = value_hue;
    hsv.saturation = value_satur;
    hsv.brightness = value_bright;

    hsv_to_rgb(&hsv, &rgb);
    pwm_vals.channel_1 = rgb.r;
    pwm_vals.channel_2 = rgb.g;
    pwm_vals.channel_3 = rgb.b;

    nrf_pwm_sequence_t seq = {
        .values.p_individual = &pwm_vals,
        .length = 4,
        .repeats = 0,
        .end_delay = 0
    };
    nrfx_pwm_simple_playback(&pwm0, &seq, 1, 0);
}


static void process_led_1(void) {
    switch (current_color_state) {
        case STATE_COLOR_NONE:
            pwm_vals.channel_0 = 0;
            break;

        case STATE_COLOR_SELECT_HUE:
            if (fade_up_yellow) {
                value += STEP_CHANGE_YELLOW_COLOR_SLOW;
                if (value >= PWM_TOP_VALUE) {
                    value = PWM_TOP_VALUE;
                    fade_up_yellow = false;
                }

            } else {
                value -= STEP_CHANGE_YELLOW_COLOR_SLOW;
                if (value < STEP_CHANGE_YELLOW_COLOR_SLOW) {
                    value = 0;
                    fade_up_yellow = true;
                }
            }
            pwm_vals.channel_0 = value;
            break;

        case STATE_COLOR_SELECT_SATURATION:
            if (fade_up_yellow) {
                value += STEP_CHANGE_YELLOW_COLOR_FAST;
                if (value >= PWM_TOP_VALUE) {
                    value = PWM_TOP_VALUE;
                    fade_up_yellow = false;
                }

            } else {
                value -= STEP_CHANGE_YELLOW_COLOR_FAST;
                if (value < STEP_CHANGE_YELLOW_COLOR_FAST) {
                    value = 0;
                    fade_up_yellow = true;
                }
            }
            pwm_vals.channel_0 = value;
            break;

        case STATE_COLOR_SELECT_BRIGHTNESS:
            pwm_vals.channel_0 = PWM_TOP_VALUE;

        default:
            break;
    }
    
    nrf_pwm_sequence_t seq = {
        .values.p_individual = &pwm_vals,
        .length = 4,
        .repeats = 0,
        .end_delay = 0
    };
    nrfx_pwm_simple_playback(&pwm0, &seq, 1, 0);
}


static void init_timers(void) {
    nrfx_clock_init(NULL);
    nrfx_clock_lfclk_start();
    app_timer_init();
    app_timer_create(&timer_debouncing, APP_TIMER_MODE_SINGLE_SHOT, handler_debouncing_timer);
    app_timer_create(&timer_double_click, APP_TIMER_MODE_SINGLE_SHOT, handler_double_click_timer);
    app_timer_create(&timer_update_color, APP_TIMER_MODE_REPEATED, handler_update_color_timer);
    app_timer_start(timer_update_color, APP_TIMER_TICKS(DELAY_INTERVAL_FADE_MS), NULL);
}


static void init_pwm(void) {
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
    
    hsv_to_rgb(&hsv, &rgb);

    pwm_vals.channel_0 = 0;
    pwm_vals.channel_1 = rgb.r;
    pwm_vals.channel_2 = rgb.g;
    pwm_vals.channel_3 = rgb.b;
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
            current_color_state = (current_color_state + 1) % STATE_COLOR_MAX;
            button_first_click = false;
            fade_up_yellow = true;
            fade_up_other = true;
            value = 0;
            app_timer_stop(timer_double_click);
        }
    }   
    button_is_pressed = true;
}


static void init_gpiote(void) {
    if (!nrfx_gpiote_is_init()) {
        nrfx_gpiote_init();
    }

    // nrfx_gpiote_in_config_t config_in = NRFX_GPIOTE_CONFIG_IN_SENSE_LOTOHI(true);
    // nrfx_gpiote_in_config_t config_in = NRFX_GPIOTE_CONFIG_IN_SENSE_TOGGLE(true);
    nrfx_gpiote_in_config_t config_in = NRFX_GPIOTE_CONFIG_IN_SENSE_HITOLO(true);
    config_in.pull = NRF_GPIO_PIN_PULLUP;
    nrfx_gpiote_in_init(BUTTON, &config_in, handler_button_pressed);
    nrfx_gpiote_in_event_enable(BUTTON, true);
}