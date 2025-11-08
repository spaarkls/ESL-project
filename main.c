#include <stdbool.h>
#include <stdint.h>
#include "nrf_delay.h"
#include "nrf_gpio.h"


#define USER_BUTTON_PIN NRF_GPIO_PIN_MAP(1, 6)
#define LED1_PIN NRF_GPIO_PIN_MAP(0, 6)
#define LED_R_PIN NRF_GPIO_PIN_MAP(0,8)
#define LED_G_PIN NRF_GPIO_PIN_MAP(1,9)
#define LED_B_PIN NRF_GPIO_PIN_MAP(0,12)

#define LEDS_NUMBER 4
#define SHORT_DELAY_MS 500
#define LONG_DELAY_MS 1000
#define HIGH 1
#define LOW 0

static void init_gpio_pins(void);
static void led_on(uint16_t indx);
static void led_off(uint16_t indx);
static void led_blink_n_times(uint16_t indx, uint16_t *counter);
static void leds_off_all(void);

static const uint16_t leds[LEDS_NUMBER] = {LED1_PIN, LED_R_PIN, LED_G_PIN, LED_B_PIN};
static const uint16_t times[LEDS_NUMBER] = {6,5,9,8};

/*
    Device ID: 6598

    Target sequence: [YYYYYYRRRRRGGGGGGGGGBBBBBBBB]
                      ^     ^    ^        ^
                      |     |    |        |
    times:            6Y    5R   9G       8B
                    
*/

int main(void) {

    init_gpio_pins();
    uint16_t index = 0;
    uint16_t counter = 0;

    while (true) {
        while (nrf_gpio_pin_read(USER_BUTTON_PIN) == 0) {
            led_blink_n_times(index % LEDS_NUMBER, &counter);
            if (counter == 0)
                ++index;
            if (nrf_gpio_pin_read(USER_BUTTON_PIN)) break; 
        }
    }

    leds_off_all();

    return 0;
}

static void led_blink_n_times(uint16_t indx, uint16_t *counter) {
    for (; (*counter) < times[indx]; (*counter)++) {
        led_off(indx);
        nrf_delay_ms(SHORT_DELAY_MS);
        if (nrf_gpio_pin_read(USER_BUTTON_PIN)) { (*counter)++; break; }
        led_on(indx);
        nrf_delay_ms(SHORT_DELAY_MS);
    }
    if ((*counter) >= times[indx]) {
        (*counter) = 0;
        led_off(indx);
    } else {
        led_on(indx);
    }
}

static void leds_off_all(void) {
    for (uint16_t i = 0; i < LEDS_NUMBER; i++) {
        nrf_gpio_pin_write(leds[i], HIGH);
    }
}

static void led_on(uint16_t indx) {
    nrf_gpio_pin_write(leds[indx], LOW);
}

static void led_off(uint16_t indx) {
    nrf_gpio_pin_write(leds[indx], HIGH);
}

static void init_gpio_pins(void) {
    nrf_gpio_cfg_input(USER_BUTTON_PIN, NRF_GPIO_PIN_PULLUP);

    for (uint16_t i = 0; i < LEDS_NUMBER; i++) {
        nrf_gpio_pin_write(leds[i], HIGH);
        nrf_gpio_cfg_output(leds[i]);
    }
}
