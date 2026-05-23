/** 
* @file display.h
* 
* display driver with basic function for an MMI
*
*/

#ifndef _inc_display
#define _inc_display

#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// Define IO pins
#define I2C_SDA0        0       // OLED Display I2C
#define I2C_SCL0        1       // OLED Display I2C

// OLED display definitions
#define I2C_OLED  		i2c0
#define I2C_OLED_BUSSPD 400000
#define I2C_ADDR        0x3C
#define OLED_WIDTH      128
#define OLED_HEIGHT     64

#define SYSMENU_ITEMS   11      // this must match with the number items
                                // in sys_menu_names (see display.c) !

/**
	@brief init I2C hardware and setup  OLED
*/
void display_init(void);

/**
	@brief clear the display
*/
void display_clear(void);

void display_contrast(uint8_t value);
void display_system_setup(void);
void display_version(int major, int minor);
void display_main_window(bool single_clock, bool trk1, bool trk2, uint8_t seq, char *tempo, char *strline1, char *strline2, char *strline3, char *strline4);
void display_sysmenu_spinner_window(uint item, uint offset_col);
void display_cal_window(uint item, char *strline1, char *strline2);
void display_tune_window(uint item, char *note, char *info);
void display_pattern_window(uint item, char *str1, char *str2, char *str3);
void display_pattern_chain_edit(char *str0, char *str1, char *str2, char *str3);
void display_pattern_selector(uint seq, uint pattern);
void display_step_selector(uint pattern, uint sel_step, char *info, char *note);

#endif