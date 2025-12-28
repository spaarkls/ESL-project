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
    STATE_COLOR_SELECT_BRIGHTNESS
} STATE_COLOR;


// MY HUE -> LAST DIGIT: 98 -> HUE = 360 * 0.98 = 352.8 (353)
// OTHER VALUES: MAX;

static volatile STATE_COLOR current_color_state = STATE_COLOR_NONE;
static struct HSV hsv = HSV_DEFAULT_CONFIG;
static struct RGB rgb = RGB_DEFAULT_CONFIG;

static volatile bool button_first_click = false;
static volatile bool button_is_pressed = false;
static volatile bool button_block = false; 
static bool fade_up = true;
static bool hue_dir_up = true;
static bool sat_dir_up = true;
static bool bright_dir_up = true;

static int16_t duty_value = 0;
static uint8_t current_led = 0;

APP_TIMER_DEF(timer_double_click);
APP_TIMER_DEF(timer_debouncing);
APP_TIMER_DEF(timer_change_fade);

static nrfx_pwm_t pwm0 = NRFX_PWM_INSTANCE(0);
static nrf_pwm_values_individual_t pwm_vals;
static nrf_pwm_sequence_t pwm_seq;

static void init_gpiote(void);
static void create_timers(void);
static void init_pwm(void);

static void handler_button_pressed(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action);
static void handler_debouncing_timer(void *p_context);
static void handler_double_click_timer(void *p_context);
static void handler_change_fade_timer(void *p_context);

static void pwm_write_channels(void) {
    pwm_vals.channel_0 = duty_value;
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

int main(void) {
    nrfx_clock_init(NULL);
    nrfx_clock_lfclk_start();
    app_timer_init();
    init_gpiote();
    init_pwm();
    create_timers();

    hsv_to_rgb(&hsv, &rgb);
    pwm_write_channels();

    while (true) {
        __WFE();
    }

    return 0;
}

static void handler_debouncing_timer(void *p_context) {
    button_block = false;
}


static void handler_double_click_timer(void *p_context) {
    button_first_click = false;
}


static void handler_change_fade_timer(void *p_context) {
    if (!button_is_pressed && current_color_state != STATE_COLOR_NONE) {
        return;
    }

    if (button_is_pressed) {
        if (nrf_gpio_pin_read(BUTTON) != 0) {
            button_is_pressed = false;
            return;
        }
    }

    switch (current_color_state) {
        case STATE_COLOR_SELECT_HUE:
            if (fade_up) {
                duty_value += STEP_CHANGE_FADE_HUE;
                hsv.hue += STEP_CHANGE_HUE;
                if (hsv.hue >= 359) fade_up = false;
            } else {
                duty_value -= STEP_CHANGE_FADE_HUE;
                hsv.hue -= STEP_CHANGE_HUE;
                if (hsv.hue <= 0) fade_up = true;
            }
            break;

        case STATE_COLOR_SELECT_SATURATION:
            if (fade_up) {
                duty_value += STEP_CHANGE_FADE_SATURATION;
                hsv.saturation += STEP_CHANGE_SATURATION;
                if (hsv.saturation >= 100) fade_up = false;
            } else {
                duty_value -= STEP_CHANGE_FADE_SATURATION;
                hsv.saturation -= STEP_CHANGE_SATURATION;
                if (hsv.saturation <= 0) fade_up = true;
            }
            break;

        case STATE_COLOR_SELECT_BRIGHTNESS:
            duty_value = PWM_TOP_VALUE;
            if (fade_up) {
                hsv.brightness += STEP_CHANGE_BRIGHT;
                if (hsv.brightness >= 100) fade_up = false;
            } else {
                hsv.brightness -= STEP_CHANGE_BRIGHT;
                if (hsv.brightness <= 0) fade_up = true;
            }
            break;

        case STATE_COLOR_NONE:
        default:
            break;
    }

    hsv_to_rgb(&hsv, &rgb);
    pwm_write_channels();
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
            current_color_state = (current_color_state + 1) % COUNT_STATE_COLOR;
            button_first_click = false;
            app_timer_stop(timer_double_click);
        }

        if (current_color_state != STATE_COLOR_NONE)
            button_is_pressed = true;
    }
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
    pwm_vals.channel_1 = 0;
    pwm_vals.channel_2 = 0;
    pwm_vals.channel_3 = 0; 
}

static void create_timers(void) {
    app_timer_create(&timer_debouncing, APP_TIMER_MODE_SINGLE_SHOT, handler_debouncing_timer);
    app_timer_create(&timer_double_click, APP_TIMER_MODE_SINGLE_SHOT, handler_double_click_timer);
    app_timer_create(&timer_change_fade, APP_TIMER_MODE_REPEATED, handler_change_fade_timer);
    app_timer_start(timer_change_fade, APP_TIMER_TICKS(DELAY_INTERVAL_FADE_MS), NULL);
}

static void init_gpiote(void) {
    if (!nrfx_gpiote_is_init()) {
        nrfx_gpiote_init();
    }

    nrfx_gpiote_in_config_t config_in = NRFX_GPIOTE_CONFIG_IN_SENSE_LOTOHI(true);
    config_in.pull = NRF_GPIO_PIN_PULLUP;
    nrfx_gpiote_in_init(BUTTON, &config_in, handler_button_pressed);
    nrfx_gpiote_in_event_enable(BUTTON, true);
}