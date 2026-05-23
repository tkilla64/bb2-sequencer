/** 
* @file pwm.h
* 
* functions for handling pwm outputs
*
*/

#ifndef _inc_pwm
#define _inc_pwm

#include <stdio.h>
#include <stdint.h>

// Define IO pins
#define TRK1_PWM_H      2       // 16-bit Track1 1V/OCT
#define TRK1_PWM_L      3       // 16-bit Track1 1V/OCT
#define TRK2_PWM_H      4       // 16-bit Track2 1V/OCT
#define TRK2_PWM_L      5       // 16-bit Track2 1V/OCT
#define TRK1_PWM_CV     6       // Track1 CV Output
#define TRK2_PWM_CV     7       // Track2 CV Output
#define TRK1_ACCENT     10      // Track1 Accent Output
#define LED_A1          14      // LED Matrix
#define LED_A2          15      // LED Matrix

// Number of PWM outputs
#define NO_OF_PWM_OUT   4
#define NO_OF_DAC_OUT	(NO_OF_PWM_OUT/2)

// Dual PWM should be always be consecutive GPIO pins, 
// set FIRST_PWM to the lowest GPIO pin number    
#define PWM_FIRST       2
#define PWM_LAST        ((PWM_FIRST+NO_OF_PWM_OUT)-1)

// Wrap counter determines PWM frequency and resolution
// Resolution: PWM_WRAP steps
// Frequency: 125 MHz / PWM_WRAP
// Value should be n^2 (16, 32, 64, 128, ...)
#define PWM_WRAP        256
#define PWM_INIT        0

/**
	@brief init pwm GPIOs
*/
void init_pwms(void);

/**
	@brief set PWM output level (2 * 8 bit)

	@param[in] channel : channel [0..NO_OF_PWM_OUT/2]
    @param[in] level   : output level [0..65535]
*/
void dual_pwm_set_out(int16_t channel, uint16_t level);

/**
	@brief set TRACK1 CV output level

	@param[in] seq   : seq track [0..1]
	@param[in] level : output level [0..255]
*/
void pwm_cv_set_out(uint8_t seq, uint8_t level);

/**
	@brief set TRACK1 ACCENT output level

	@param[in] level : output level [0..255]
*/
void trk1_accent_set_out(uint8_t level);

/**
	@brief set LED_A1 intensity

	@param[in] level : output level [0..255]
*/
void led_a1_set_out(uint8_t level);

/**
	@brief set LED_A2 intensity

	@param[in] level : output level [0..255]
*/
void led_a2_set_out(uint8_t level);

#endif
