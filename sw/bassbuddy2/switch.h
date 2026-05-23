/** 
* @file switch.h
* 
* functions for handling switches and buttons
*
*/

#ifndef _inc_switch
#define _inc_switch

#include <stdio.h>
#include "pico/stdlib.h"

// Number of buttons
#define NO_OF_BUTTONS	6

// Define IO pins
#define TRACK_N         12      // TRACK button
#define MENU_N          13      // MENU button
#define PATTERN_N       16      // PATTERN button
#define ENC_SW_N        17      // Encoder Switch button
#define ENC_B           24      // Encoder Rotary Switch
#define ENC_A           25      // Encoder Rotary Switch
#define STEP_N          26      // STEP button
#define EDIT_N          27      // EDIT button

// Button bitmasks, needed
#define TRACK_EV	0x0001	// Button press event
#define MENU_EV		0x0002
#define EDIT_EV		0x0004
#define STEP_EV		0x0008
#define PATTERN_EV	0x0010
#define ENC_SW_EV	0x0020
#define ALL_EV		0x003F

#define BUTTON_POLL 10U 	// ms between button poll calls
#define ENC_POLL	2		// Encoder poll interval (ms)
#define STEP_NOTCH  2		// No of steps per notch

/**
	@brief init button GPIOs
*/
void init_button_gpios(void);

/**
	@brief poll encoder value
*/
void encoder_poll(void);

/**
	@brief get encoder value
*
* 	@return int32_t
*	@retval encoder value
*/
int32_t encoder_get_data(void);

/**
	@brief preset encoder to a value and set limits

	@param[in] setvalue : encoder value
	@param[in] min_limit : encoder min limit
	@param[in] max_limit : encoder max limit

*/
void encoder_set_data(int32_t setvalue, int32_t min_limit, int32_t max_limit);

/**
	@brief constrain value to limits

	@param[in] x : value to check
	@param[in] some_minimum : min limit
	@param[in] some_maximum : max limit

	@return int32_t
 	@retval value within limits

*/
int32_t constrain(int32_t x, int32_t some_minimum, int32_t some_maximum);

/**
	@brief poll, debounce buttons ans set event
*/
void buttons_poll(void);

/**
	@brief readout button event mask
*/
int16_t read_button_event(void);

/**
	@brief preset encoder to a value

	@param[in] clear_mask : mask with which events to clear
*/
void clear_button_event(uint16_t clear_mask);

/**
	@brief TRACK button debounced
*
* 	@return bool
*	@retval true if pressed
*	@retval false if not pressed or held pressed
*/
bool read_track_button(void);

/**
	@brief MENU button debounced
*
* 	@return bool
*	@retval true if pressed
*	@retval false if not pressed or held pressed
*/
bool read_menu_button(void);

/**
	@brief EDIT button debounced
    
*
* 	@return bool
*	@retval true if pressed
*	@retval false if not pressed or held pressed
*/
bool read_edit_button(void);

/**
	@brief STEP button debounced
*
* 	@return bool
*	@retval true if pressed
*	@retval false if not pressed or held pressed
*/
bool read_step_button(void);

/**
	@brief PATTERN button debounced
*
* 	@return bool
*	@retval true if pressed
*	@retval false if not pressed or held pressed
*/
bool read_pattern_button(void);

/**
	@brief ENC_SW button debounced
*
* 	@return bool
*	@retval true if pressed
*	@retval false if not pressed or held pressed
*/
bool read_enc_sw_button(void);

/**
	@brief return status of TRACK button
*
* 	@return bool
*	@retval true if held pressed
*	@retval false if not pressed
*/
bool read_track_button_status(void);

/**
	@brief return status of MENU button
*
* 	@return bool
*	@retval true if held pressed
*	@retval false if not pressed
*/
bool read_menu_button_status(void);

/**
	@brief return status of EDIT button
*
* 	@return bool
*	@retval true if held pressed
*	@retval false if not pressed
*/
bool read_edit_button_status(void);

/**
	@brief return status of STEP button
*
* 	@return bool
*	@retval true if held pressed
*	@retval false if not pressed
*/
bool read_step_button_status(void);

/**
	@brief return status of PATTERN button
*
* 	@return bool
*	@retval true if held pressed
*	@retval false if not pressed
*/
bool read_pattern_button_status(void);

/**
	@brief return status of ENC_SW button
*
* 	@return bool
*	@retval true if held pressed
*	@retval false if not pressed
*/
bool read_enc_sw_button_status(void);

#endif