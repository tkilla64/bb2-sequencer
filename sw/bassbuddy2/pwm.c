/** 
* @file pwm.c
* 
* functions for handling pwm outputs
*
*/

#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pwm.h"

void init_pwms(void)
{
    uint slice_num;

    // setup Dual PWMs
    for (int iopin = PWM_FIRST ; iopin < PWM_LAST+1 ; iopin++)
    {
        slice_num = pwm_gpio_to_slice_num(iopin);
        gpio_set_function(iopin, GPIO_FUNC_PWM);
        pwm_set_wrap(slice_num, PWM_WRAP-1);
        // Set a default value
        pwm_set_gpio_level(iopin, PWM_INIT);
        // Start the PWM
        pwm_set_enabled(slice_num, true);
    }
    // setup single PWMs for CV and ACCENT
    slice_num = pwm_gpio_to_slice_num(TRK1_PWM_CV);
    gpio_set_function(TRK1_PWM_CV, GPIO_FUNC_PWM);
    pwm_set_wrap(slice_num, PWM_WRAP-1);
    pwm_set_gpio_level(TRK1_PWM_CV, PWM_INIT);
    pwm_set_enabled(slice_num, true);
    slice_num = pwm_gpio_to_slice_num(TRK2_PWM_CV);
    gpio_set_function(TRK2_PWM_CV, GPIO_FUNC_PWM);
    pwm_set_wrap(slice_num, PWM_WRAP-1);
    pwm_set_gpio_level(TRK2_PWM_CV, PWM_INIT);
    pwm_set_enabled(slice_num, true);
    slice_num = pwm_gpio_to_slice_num(TRK1_ACCENT);
    gpio_set_function(TRK1_ACCENT, GPIO_FUNC_PWM);
    pwm_set_wrap(slice_num, PWM_WRAP-1);
    pwm_set_gpio_level(TRK1_ACCENT, PWM_INIT);
    pwm_set_enabled(slice_num, true);
    // LED PWMs
    slice_num = pwm_gpio_to_slice_num(LED_A1);
    gpio_set_function(LED_A1, GPIO_FUNC_PWM);
    pwm_set_wrap(slice_num, PWM_WRAP-1);
    pwm_set_gpio_level(LED_A1, PWM_INIT);
    pwm_set_enabled(slice_num, true);
    gpio_set_drive_strength(LED_A1, GPIO_DRIVE_STRENGTH_12MA);
    slice_num = pwm_gpio_to_slice_num(LED_A2);
    gpio_set_function(LED_A2, GPIO_FUNC_PWM);
    pwm_set_wrap(slice_num, PWM_WRAP-1);
    pwm_set_gpio_level(LED_A2, PWM_INIT);
    pwm_set_enabled(slice_num, true);
    gpio_set_drive_strength(LED_A2, GPIO_DRIVE_STRENGTH_12MA);
}

// Set channel to level
// Channels are paired 0-1, 2-3, ... 
// where the 1st ch is Upper byte and 2nd Lower byte
void dual_pwm_set_out(int16_t channel, uint16_t level)
{
    pwm_set_gpio_level(((channel*2)+PWM_FIRST), level >> 8);
    pwm_set_gpio_level(((channel*2)+PWM_FIRST+1), level & 0x00FF);
}

// Set TRACK1 CV output level
void pwm_cv_set_out(uint8_t seq, uint8_t level)
{
    if (seq == 0)
        pwm_set_gpio_level(TRK1_PWM_CV, level);
    else
        pwm_set_gpio_level(TRK2_PWM_CV, level);
}

// Set TRACK1 ACCENT output level
void trk1_accent_set_out(uint8_t level)
{
    pwm_set_gpio_level(TRK1_ACCENT, level);
}

// Set LED_A1 intensity
void led_a1_set_out(uint8_t level)
{
    pwm_set_gpio_level(LED_A1, level);
}

// Set LED_A2 intensity
void led_a2_set_out(uint8_t level)
{
    pwm_set_gpio_level(LED_A2, level);
}