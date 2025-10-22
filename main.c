#include <stdbool.h>
#include <stdint.h>
#include "nrf_delay.h"
#include "boards.h"

#define SHORT_DELAY_MS 500
#define LONG_DELAY_MS 1000

typedef struct {
    short times[LEDS_NUMBER];
    short blink_delay_ms;

} shared_state_t;

void led_blink_n_times(short led_indx, shared_state_t *state);

int main(void) {

    bsp_board_init(BSP_INIT_LEDS);
    shared_state_t led_state = {{6,5,9,8}, SHORT_DELAY_MS};

    while (true) {
        for (short led = 0; led < LEDS_NUMBER; led++) {
            led_blink_n_times(led, &led_state);
            nrf_delay_ms(LONG_DELAY_MS);
        }
    }
    return 0;
}

void led_blink_n_times(short led_index, shared_state_t *state) {
    for (short count = 0; count < state->times[led_index]; count++) {
        bsp_board_led_on(led_index);
        nrf_delay_ms(state->blink_delay_ms);
        bsp_board_led_off(led_index);
        nrf_delay_ms(state->blink_delay_ms);
    }
}