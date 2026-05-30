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
#include "flash.h"
#include "cli.h"


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
    bool need_write;
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
static void process_rgb(void *ctx);
static void update_value_with_limits(uint16_t *value, uint16_t step, uint16_t max_value, bool *fade_up);
static void update_pwm_channels(uint16_t value_ch0, uint16_t value_ch1, uint16_t value_ch2, uint16_t value_ch3);

int main(void) {
    struct app_color app;
    init_app(&app);
    g_app = &app;

    init_timers(&app);
    init_gpiote(&app);
    flash_read(FLASH_ADDRESS, &app.hsv, sizeof(struct HSV));

    usb_init();

    init_pwm(&app);


    while (true) {
        usb_process();
        __WFE();
    }

    return 0;
}

static void handler_debouncing_timer(void *ctx) {
    UNUSED(ctx);
    button_block = false;
}


static void handler_double_click_timer(void *ctx) {
    UNUSED(ctx);
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

    if (button_is_pressed && app->current_state != STATE_COLOR_NONE) {
        process_rgb(app);
    }

    update_pwm_channels(app->state_value, app->rgb.red, app->rgb.green, app->rgb.blue);
    nrfx_pwm_simple_playback(&pwm0, &pwm_seq, 1, 0);
}


static void process_led_1(void *ctx) {
    VALID_PTR(ctx);
    struct app_color *app = ctx;
    
    switch (app->current_state) {
        case STATE_COLOR_NONE:
            if (app->need_write) {
                app->need_write = false;
                flash_write(FLASH_ADDRESS, &app->hsv, sizeof(struct HSV));
            }
            app->state_value = 0;
            break;

        case STATE_COLOR_SELECT_HUE:
            update_value_with_limits(&app->state_value, STEP_CHANGE_YELLOW_COLOR_SLOW, PWM_TOP_VALUE, &app->fade_up_yellow);
            break;

        case STATE_COLOR_SELECT_SATURATION:
            update_value_with_limits(&app->state_value, STEP_CHANGE_YELLOW_COLOR_FAST, PWM_TOP_VALUE, &app->fade_up_yellow);
            break;

        case STATE_COLOR_SELECT_BRIGHTNESS:
            app->need_write = true;
            app->state_value = PWM_TOP_VALUE;
            break;

        default:
            break;
    }
}


static void process_rgb(void *ctx) {
    VALID_PTR(ctx);
    struct app_color *app = ctx;

    switch (app->current_state) {
        case STATE_COLOR_SELECT_HUE:
            update_value_with_limits(&app->hsv.hue, STEP_CHANGE_HUE, MAX_VALUE_HUE, &app->fade_up_other);
            break;

        case STATE_COLOR_SELECT_SATURATION:
            update_value_with_limits(&app->hsv.saturation, STEP_CHANGE_SATURATION, MAX_VALUE_SATURATION, &app->fade_up_other);
            break;

        case STATE_COLOR_SELECT_BRIGHTNESS:
            update_value_with_limits(&app->hsv.brightness, STEP_CHANGE_BRIGHT, MAX_VALUE_BRIGHTNESS, &app->fade_up_other);
            break;

        default:
            break;
    }

    color_hsv_to_rgb(&app->hsv, &app->rgb);
}


static void update_value_with_limits(uint16_t *value, uint16_t step, uint16_t max_value, bool *fade_up) {
    if (*fade_up) {
        *value += step;
        if (*value >= max_value) {
            *value = max_value;
            *fade_up = false;
        }
    } else {
        *value -= step;
        if (*value <= step) {
            *value = 0;
            *fade_up = true;
        }
    }
}


static void update_pwm_channels(uint16_t value_ch0, uint16_t value_ch1, uint16_t value_ch2, uint16_t value_ch3) {
    pwm_vals.channel_0 = value_ch0;
    pwm_vals.channel_1 = value_ch1;
    pwm_vals.channel_2 = value_ch2;
    pwm_vals.channel_3 = value_ch3;
}



static void init_app(struct app_color *app) {
    VALID_PTR(app);

    color_init_hsv(&app->hsv);
    color_init_rgb(&app->rgb);

    app->current_state = STATE_COLOR_NONE;
    app->fade_up_other = true;
    app->fade_up_yellow = true;
    app->state_value = 0;
    app->need_write = false;
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
    update_pwm_channels(0, app->rgb.red, app->rgb.green, app->rgb.blue);

    pwm_seq.values.p_individual = &pwm_vals;
    pwm_seq.length = 4;
    pwm_seq.repeats = 0;
    pwm_seq.end_delay = 0;
}


static void handler_button_pressed(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action) {
    UNUSED(action);
    
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
    UNUSED(ctx);

    if (!nrfx_gpiote_is_init()) {
        nrfx_gpiote_init();
    }

    nrfx_gpiote_in_config_t config_in = NRFX_GPIOTE_CONFIG_IN_SENSE_HITOLO(true);
    config_in.pull = NRF_GPIO_PIN_PULLUP;
    nrfx_gpiote_in_init(BUTTON, &config_in, handler_button_pressed);
    nrfx_gpiote_in_event_enable(BUTTON, true);
}