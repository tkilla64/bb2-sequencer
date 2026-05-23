/** 
* @file switch.c
* 
* functions for handling switches and buttons
*
*/

#include <stdio.h>
#include "switch.h"

bool debounce_button(int index, int inputpin);
void setup_button_gpio(int inputpin);

// init button gpios
void init_button_gpios(void)
{
    setup_button_gpio(TRACK_N);
    setup_button_gpio(MENU_N);
    setup_button_gpio(PATTERN_N);
    setup_button_gpio(STEP_N);
    setup_button_gpio(EDIT_N);
    setup_button_gpio(ENC_A);
    setup_button_gpio(ENC_B);
    setup_button_gpio(ENC_SW_N);
}

volatile int32_t encoder_value = 0;
volatile int32_t encoder_min = 0;
volatile int32_t encoder_max = 1;
// encoder function, should be polled every 2 ms
void encoder_poll(void)
{
    static bool pos = 0;

    if (!gpio_get(ENC_A) != pos)
        if (!gpio_get(ENC_B) != pos)
            encoder_value--;
        else
            encoder_value++;

    pos = !gpio_get(ENC_A);
    encoder_value = constrain(encoder_value, encoder_min, encoder_max);
}

// get encoder value
int32_t encoder_get_data(void)
{
    return encoder_value / STEP_NOTCH;
}

// set encoder value
void encoder_set_data(int32_t setvalue, int32_t min_limit, int32_t max_limit)
{
    encoder_value = setvalue * STEP_NOTCH;
    encoder_min = min_limit * STEP_NOTCH;
    encoder_max = max_limit * STEP_NOTCH;
}

// Make sure x is within the limits
int32_t constrain(int32_t x, int32_t some_minimum, int32_t some_maximum) {
    return (x < some_minimum ? some_minimum : x > some_maximum ? some_maximum : x);
}

// Push-button functions
volatile int16_t button_event = 0;
void buttons_poll(void)
{
    if (debounce_button(0, TRACK_N)) 
        button_event |= TRACK_EV;
    if (debounce_button(1, MENU_N))
        button_event |= MENU_EV;
    if (debounce_button(2, EDIT_N))
        button_event |= EDIT_EV;
    if (debounce_button(3, STEP_N)) 
        button_event |= STEP_EV;
    if (debounce_button(4, PATTERN_N))
        button_event |= PATTERN_EV;
    if (debounce_button(5, ENC_SW_N))
        button_event |= ENC_SW_EV;
}

int16_t read_button_event(void)
{
    return button_event;
}

void clear_button_event(uint16_t clear_mask)
{
    button_event &= ~clear_mask;
}

bool read_track_button(void)
{
    bool ret_val = (button_event & TRACK_EV);
    button_event &= ~TRACK_EV;
    return ret_val;
}

bool read_menu_button(void)
{
    bool ret_val = (button_event & MENU_EV);
    button_event &= ~MENU_EV;
    return ret_val;
}

bool read_edit_button(void)
{
    bool ret_val = (button_event & EDIT_EV);
    button_event &= ~EDIT_EV;
    return ret_val;
}

bool read_step_button(void)
{
    bool ret_val = (button_event & STEP_EV);
    button_event &= ~STEP_EV;
    return ret_val;
}

bool read_pattern_button(void)
{
    bool ret_val = (button_event & PATTERN_EV);
    button_event &= ~PATTERN_EV;
    return ret_val;
}

bool read_enc_sw_button(void)
{
    bool ret_val = (button_event & ENC_SW_EV);
    button_event &= ~ENC_SW_EV;
    return ret_val;
}

// Raw status from GPIOs
bool read_track_button_status(void)
{
    return !gpio_get(TRACK_N);
}

bool read_menu_button_status(void)
{
    return !gpio_get(MENU_N);
}

bool read_edit_button_status(void)
{
    return !gpio_get(EDIT_N);
}

bool read_step_button_status(void)
{
    return !gpio_get(STEP_N);
}

bool read_pattern_button_status(void)
{
    return !gpio_get(PATTERN_N);
}

bool read_enc_sw_button_status(void)
{
    return !gpio_get(ENC_SW_N);
}

// Debounce (active low) buttons, Jack Ganssle style
bool debounce_button(int index, int inputpin) 
{
  static uint16_t state[NO_OF_BUTTONS] = { 0 };
  state[index] = (state[index]<<1) | gpio_get(inputpin) | 0xffe0;
  return (bool)(state[index] == 0xfff0);
}

// Setup pin to GPIO input without pullup
void setup_button_gpio(int pin)
{
  gpio_init(pin);    
  gpio_set_dir(pin, GPIO_IN);
  gpio_disable_pulls(pin);    
}