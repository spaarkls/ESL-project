#include <stdbool.h>
#include <stdint.h>
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrfx_gpiote.h"
#include "nrfx_clock.h"
#include "nrfx_pwm.h"
#include "app_timer.h"

#define BUTTON NRF_GPIO_PIN_MAP(1, 6)

#define LED_Y NRF_GPIO_PIN_MAP(0, 6)
#define LED_R NRF_GPIO_PIN_MAP(0, 8)
#define LED_G NRF_GPIO_PIN_MAP(1, 9)
#define LED_B NRF_GPIO_PIN_MAP(0, 12)

#define TOP_PWM_VALUE 1000
#define STEP_CHANGE_FADE 10
#define LEDS_COUNT 4
#define INTERVAL_FADE_MS 10
#define DEBOUNCE_MS 50
#define DOUBLE_CLICK_MS 400

APP_TIMER_DEF(fading_timer);
APP_TIMER_DEF(debouncing_timer);
APP_TIMER_DEF(double_click_timer);


static int16_t duty_value = 0;
static uint8_t current_led = 0;
static uint8_t times = 0;
static uint8_t curr_blink_indx = 0;
static uint8_t times_blink[LEDS_COUNT] = {6, 5, 9, 8};

volatile bool fade_up = true;
volatile bool button_is_pressed = false;
volatile bool button_block = false;
volatile bool button_first_click = false;

static nrfx_pwm_t pwm0 = NRFX_PWM_INSTANCE(0);
static nrf_pwm_values_individual_t pwm_vals;
static nrf_pwm_sequence_t pwm_seq;


static void init_pwm(void);
static void init_gpiote(void); 
static void handler_button_pressed(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action);
static void handler_timer(void *p_context);
static void handler_timer_debouncing(void *p_context);
static void handler_timer_double_click(void *p_context);
static void pwm_change(void);

int main(void) {
    nrfx_clock_init(NULL);
    nrfx_clock_lfclk_start();
    app_timer_init();
    init_pwm();
    init_gpiote();

    while (true) {
        __WFE();        
    }

    return 0;
}

static void handler_timer(void *p_context) {
    if (!button_is_pressed) { 
        return; 
    } 

    if (fade_up) {
        duty_value += STEP_CHANGE_FADE;
        if (duty_value >= TOP_PWM_VALUE) {
            duty_value = TOP_PWM_VALUE;
            fade_up = false;
        }
    
    } else {
        if (duty_value > STEP_CHANGE_FADE)
            duty_value -= STEP_CHANGE_FADE;
        else {
            duty_value = 0;
            fade_up = true;
            times++;
            if (times == times_blink[curr_blink_indx]) {
                current_led = (current_led + 1) % LEDS_COUNT;
                times = 0;
                curr_blink_indx = (curr_blink_indx + 1) % LEDS_COUNT;
            }
        }
    }

    pwm_vals.channel_0 = 0;
    pwm_vals.channel_1 = 0;
    pwm_vals.channel_2 = 0;
    pwm_vals.channel_3 = 0;

    switch (current_led) {
        case 0: pwm_vals.channel_0 = duty_value; break;
        case 1: pwm_vals.channel_1 = duty_value; break;
        case 2: pwm_vals.channel_2 = duty_value; break;
        case 3: pwm_vals.channel_3 = duty_value; break;
        default:
            break;
    }
    
    nrf_pwm_sequence_t seq = {
        .values.p_individual = &pwm_vals,
        .length = LEDS_COUNT,
        .repeats = 0,
        .end_delay = 0
    };

    nrfx_pwm_simple_playback(&pwm0, &seq, 1, 0);
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
        .top_value = TOP_PWM_VALUE,
        .load_mode = NRF_PWM_LOAD_INDIVIDUAL,
        .step_mode = NRF_PWM_STEP_AUTO
    };

    nrfx_pwm_init(&pwm0, &config_pwm, NULL);
    pwm_vals.channel_0 = 0;
    pwm_vals.channel_1 = 0;
    pwm_vals.channel_2 = 0;
    pwm_vals.channel_3 = 0;
    
    app_timer_create(&fading_timer, APP_TIMER_MODE_REPEATED, handler_timer);
}

static void handler_timer_debouncing(void *p_context) {
    button_block = false;
}

static void handler_timer_double_click(void *p_context) {
    button_first_click = false;
}

static void pwm_change(void) {
    app_timer_start(fading_timer, APP_TIMER_TICKS(INTERVAL_FADE_MS), NULL);
}

static void handler_button_pressed(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action) {
    if (pin == BUTTON) {
        
        if (button_block) {
            return;
        }
        button_block = true;
        app_timer_start(debouncing_timer, APP_TIMER_TICKS(DEBOUNCE_MS), NULL);
        
        if (!button_first_click) {
            button_first_click = true;
            app_timer_start(double_click_timer, APP_TIMER_TICKS(DOUBLE_CLICK_MS), NULL);
            
        } else {
            button_is_pressed = !button_is_pressed;
            button_first_click = false;
            app_timer_stop(double_click_timer);
            if (button_is_pressed) {
                pwm_change();
            }

        }
    }
}

static void init_gpiote(void) {
    if (!nrfx_gpiote_is_init())
        nrfx_gpiote_init();

    nrfx_gpiote_in_config_t config_in = NRFX_GPIOTE_CONFIG_IN_SENSE_LOTOHI(true);
    config_in.pull = NRF_GPIO_PIN_PULLUP;
    nrfx_gpiote_in_init(BUTTON, &config_in, handler_button_pressed);
    nrfx_gpiote_in_event_enable(BUTTON, true);

    app_timer_create(&debouncing_timer, APP_TIMER_MODE_SINGLE_SHOT, handler_timer_debouncing);
    app_timer_create(&double_click_timer, APP_TIMER_MODE_SINGLE_SHOT, handler_timer_double_click);
}
