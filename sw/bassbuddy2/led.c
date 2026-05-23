/** 
* @file led.c
* 
* functions for handling switches and buttons
*
*/

#include <stdio.h>
#include <stdint.h>
#include "pwm.h"
#include "led.h"

// Setup LED states
static led_matrix_t led_pairs[MAX_LED_PAIRS] = {
    { off, 0, 0, 0, LED_K1 }, // PATTERN
    { off, 0, 0, 0, LED_K2 }, // STEP
    { off, 0, 0, 0, LED_K3 }, // EDIT
    { off, 0, 0, 0, LED_K4 }, // TRACK1
    { off, 0, 0, 0, LED_K4 }  // TRACK2
};

// init LED GPIOs
void led_init_gpios(void) {
    gpio_init(LED_K1);
    gpio_set_dir(LED_K1, GPIO_OUT);
    gpio_set_drive_strength(LED_K1, GPIO_DRIVE_STRENGTH_12MA);
    gpio_init(LED_K2);
    gpio_set_dir(LED_K2, GPIO_OUT);
    gpio_set_drive_strength(LED_K2, GPIO_DRIVE_STRENGTH_12MA);
    gpio_init(LED_K3);
    gpio_set_dir(LED_K3, GPIO_OUT);
    gpio_set_drive_strength(LED_K3, GPIO_DRIVE_STRENGTH_12MA);
    gpio_init(LED_K4);
    gpio_set_dir(LED_K4, GPIO_OUT);
    gpio_set_drive_strength(LED_K4, GPIO_DRIVE_STRENGTH_12MA);
}

// cycle through LED matrix, called periodically
// from the timer ISR
volatile int cycle = 0;
void led_cycle_matrix(void) {
    // activate cathone pin for current cycle, but since the 
    // TRACK1 and TRACK2 LEDs share the same cathode pin, 
    // we need to activate it for both cycles. 
    for (int i = 0 ; i < MAX_LED_PAIRS ; i++) {
        gpio_put(led_pairs[i].led_cathode_pin, 
            (i == cycle) ? 0 : (cycle >= LED_TRACK1 && i >= LED_TRACK1) ? 0 : 1);
    }
    switch (led_pairs[cycle].led_mode) {
        case green_fade:
            led_pairs[cycle].curr_red_intensity = 0;
            led_pairs[cycle].curr_grn_intensity = 255;
            led_pairs[cycle].direction = -1;
            led_pairs[cycle].led_mode = green_fade_run;
            break;
        case green_fade_run:
            if (led_pairs[cycle].curr_grn_intensity > LED_RAMP_STEP) {
                led_pairs[cycle].curr_grn_intensity -= LED_RAMP_STEP;
            } else {
                led_pairs[cycle].led_mode = off;
                led_pairs[cycle].direction = 0;
            }
            break;
        case red_fade:
            led_pairs[cycle].curr_red_intensity = 255;
            led_pairs[cycle].curr_grn_intensity = 0;
            led_pairs[cycle].direction = -1;
            led_pairs[cycle].led_mode = red_fade_run;
            break;
        case red_fade_run:    
            if (led_pairs[cycle].curr_red_intensity > LED_RAMP_STEP) {
                led_pairs[cycle].curr_red_intensity -= LED_RAMP_STEP;
            } else {
                led_pairs[cycle].led_mode = off;
                led_pairs[cycle].direction = 0;
            }
            break;        
        case amber_fade:
            led_pairs[cycle].curr_red_intensity = 255;
            led_pairs[cycle].curr_grn_intensity = 255;
            led_pairs[cycle].direction = -1;
            led_pairs[cycle].led_mode = amber_fade_run;
            break;
        case amber_fade_run:
            if (led_pairs[cycle].curr_red_intensity > LED_RAMP_STEP) {
                led_pairs[cycle].curr_red_intensity -= LED_RAMP_STEP;
                led_pairs[cycle].curr_grn_intensity -= LED_RAMP_STEP;
            } else {
                led_pairs[cycle].led_mode = off;
                led_pairs[cycle].direction = 0;
            }
            break;
        case off:
            led_pairs[cycle].curr_red_intensity = 0;
            led_pairs[cycle].curr_grn_intensity = 0;            
            break;
        case green:
            led_pairs[cycle].curr_red_intensity = 0;
            led_pairs[cycle].curr_grn_intensity = 255;
            break;
        case amber:
            led_pairs[cycle].curr_red_intensity = 255;
            led_pairs[cycle].curr_grn_intensity = 255;
            break;
        case red:
            led_pairs[cycle].curr_red_intensity = 255;
            led_pairs[cycle].curr_grn_intensity = 0;
        case pulsing_green:
        case pulsing_red_amber:
        default:
            break;
    }
    // update LED PWMs, note that TRACK LEDs are both the same color
    led_a1_set_out((cycle >= LED_TRACK1) 
        ? led_pairs[cycle].curr_red_intensity 
        : (led_pairs[cycle].curr_red_intensity/RLED_INT_RATIO));
    led_a2_set_out(led_pairs[cycle].curr_grn_intensity);
    cycle++;
    cycle %= MAX_LED_PAIRS;
}

void led_set_status(int led_id, uint8_t led_mode) {
    led_pairs[led_id].led_mode = led_mode;
}