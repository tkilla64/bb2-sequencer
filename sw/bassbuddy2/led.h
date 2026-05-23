/** 
* @file led.h
* 
* functions for handling LEDs
*
*/

#ifndef _inc_led
#define _inc_led

#include <stdio.h>
#include "pico/stdlib.h"

/*
 * LED Matrix    
 */

// Define IO pins 
#define LED_K1          23  // LED Matrix
#define LED_K2          22  // LED Matrix
#define LED_K3          9   // LED Matrix
#define LED_K4          11  // LED Matrix

#define LED_PATTERN     0
#define LED_STEP        1
#define LED_EDIT        2
#define LED_TRACK1      3   // red
#define LED_TRACK2      4   // green
#define MAX_LED_PAIRS   5   // No of LED pairs

// Red LED has a higher intesity than the Green LED
// so we use a ratio to calculate the PWM value
#define RLED_INT_RATIO  3   // Red LED intensity ratio    
#define LED_CYCLE       4   // time between LED cycles (ms)
#define LED_RAMP_STEP   50  // rampdown steps per cycle

// Button LED states
enum bi_led_mode {      // red and green LEDs AAK
    off,                // both off
    green,              // green is on
    amber,              // red and green are on
    red,                // red is on
    pulsing_green,      // green is pulsing
    pulsing_red_amber,  // red is on and green pulsing
    green_fade,         // start at full on 
    green_fade_run,     // ...and fade out -> off
    red_fade,           // start at full on
    red_fade_run,       // ...and fade out -> off
    amber_fade,         // start at full on
    amber_fade_run,     // ...and fade out -> off
};

typedef struct led_matrix
{
    uint8_t led_mode;   // enum bi_led_mode
    uint8_t curr_red_intensity; // 0=off 255=max
    uint8_t curr_grn_intensity; // 0=off 255=max
    int8_t  direction;  // 0=no ramp, 1=ramp up, -1=ramp down
    int8_t  led_cathode_pin;    // GPIO pin for LED cathode
} led_matrix_t;

/**
	@brief init LED GPIOs
*/
void led_init_gpios(void);

/**
	@brief LED cycle callout
*/
void led_cycle_matrix(void);

/**
	@brief set the visual mode of the LED

	@param[in] led_id   : LED index
    @param[in] led_mode : LED visual mode
    
*/
void led_set_status(int led_id, uint8_t led_mode);

#endif