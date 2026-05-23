/** 
* @file display.c
* 
* display driver with basic function for an MMI
*
*/

#include <stdio.h>
#include "display.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include "switch.h"
#include "string.h"

// System Menu
// Use 3 or 4 character named for menu items and don't
// forget to edit SYSMENU_ITEMS in display.h
const char sys_menu_names[SYSMENU_ITEMS][6] = {
    "NEW*", "LOAD", "SAVE", "COPY",
    "CLK",  "SEQ",  "CAL1", "CAL2", 
    "TUN1", "TUN2", "FACT",
};

ssd1306_t disp;

void display_init(void)
{
    // Init I2C
    i2c_init(I2C_OLED, I2C_OLED_BUSSPD);
    gpio_set_function(I2C_SDA0, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL0, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA0);
    gpio_pull_up(I2C_SCL0);

    disp.external_vcc=false;
    ssd1306_init(&disp, OLED_WIDTH, OLED_HEIGHT, I2C_ADDR, I2C_OLED);
}

void display_contrast(uint8_t value)
{
    ssd1306_contrast(&disp, value);
}

void display_clear(void)
{
    ssd1306_clear(&disp);
    ssd1306_show(&disp);
}

void display_system_setup(void)
{
    ssd1306_clear(&disp);
    ssd1306_draw_empty_square(&disp, 12, 22, OLED_WIDTH-24, OLED_HEIGHT-44);
    ssd1306_draw_string(&disp, 22, 30, 1, "Parameter Setup");
                                           
    ssd1306_show(&disp);
}

void display_version(int major, int minor)
{
    char string[10];
    sprintf(string, "Ver %d.%02d", major, minor);
    ssd1306_clear(&disp);
    ssd1306_draw_empty_square(&disp, 12, 22, OLED_WIDTH-24, OLED_HEIGHT-44);
    ssd1306_draw_string(&disp, 18, 25, 2, string);
    ssd1306_show(&disp);
}

void display_main_window(bool single_clock, bool trk1, bool trk2, uint8_t seq, char *tempo, char *strline1, char *strline2, char *strline3, char *strline4)
{
    ssd1306_clear(&disp);
    // Top area
    if (single_clock)
        ssd1306_draw_empty_square(&disp, 0, 0, 82, 12);
    else
        ssd1306_draw_empty_square(&disp, 0, 0, 127, 12);

    ssd1306_draw_string(&disp, 3, 3, 1, tempo);

    // Track marker dot
    if (seq == 0)
        ssd1306_draw_square(&disp, 118, 40, 8, 8);
    else
        ssd1306_draw_square(&disp, 118, 15, 8, 8);

    // Track2 area
    ssd1306_draw_empty_square(&disp, 0, 13, 127, 24);
    if (trk2) {
        ssd1306_draw_string(&disp, 3, 17, 1, strline3);
        ssd1306_draw_string(&disp, 3, 26, 1, strline4);
    }
    else
        ssd1306_draw_string(&disp, 30, 23, 1, "Track2 MUTE");

    // Track1 area
    ssd1306_draw_empty_square(&disp, 0, 38, 127, 24);
    if (trk1) {
        ssd1306_draw_string(&disp, 3, 42, 1, strline1);
        ssd1306_draw_string(&disp, 3, 51, 1, strline2);
    }
    else
        ssd1306_draw_string(&disp, 30, 48, 1, "Track1 MUTE");
    
    ssd1306_show(&disp);
}

void display_sysmenu_spinner_window(uint item, uint offset_col)
{
    uint pos = item + SYSMENU_ITEMS;
    uint center_col = strlen(sys_menu_names[pos%SYSMENU_ITEMS]) * 6;

    ssd1306_clear(&disp);
    ssd1306_draw_empty_square(&disp, offset_col, 0, offset_col+62, 63);
    // Menu spinner
    ssd1306_draw_string(&disp, offset_col+22, 4, 1, sys_menu_names[(pos-2)%SYSMENU_ITEMS]);
    ssd1306_draw_string(&disp, offset_col+22, 14, 1, sys_menu_names[(pos-1)%SYSMENU_ITEMS]);
    ssd1306_draw_string(&disp, offset_col+(32-center_col), 24, 2, sys_menu_names[pos%SYSMENU_ITEMS]); // center row
    ssd1306_draw_string(&disp, offset_col+22, 42, 1, sys_menu_names[(pos+1)%SYSMENU_ITEMS]);
    ssd1306_draw_string(&disp, offset_col+22, 52, 1, sys_menu_names[(pos+2)%SYSMENU_ITEMS]);
    ssd1306_show(&disp);
}

void display_cal_window(uint item, char *str1, char *str2)
{
    ssd1306_clear(&disp);
    ssd1306_draw_empty_square(&disp, 0, 0, 127, 63);
    ssd1306_draw_string(&disp, 42, 4, 2, sys_menu_names[item]); // title
    ssd1306_draw_string(&disp, 2, 24, 1, str1);
    ssd1306_draw_string(&disp, 36, 34, 1, str2);
    ssd1306_draw_string(&disp, 7, 50, 1, "Press knob to store");
    ssd1306_show(&disp);
}

void display_tune_window(uint item, char *note, char *info)
{
    ssd1306_clear(&disp);
    ssd1306_draw_empty_square(&disp, 0, 0, 127, 63);
    ssd1306_draw_string(&disp, 42, 4, 2, sys_menu_names[item]); // title
    ssd1306_draw_string(&disp, 45, 24, 2, note);
    ssd1306_draw_string(&disp, 7, 50, 1, info);
    ssd1306_show(&disp);
}

void display_pattern_window(uint item, char *str1, char *str2, char *str3)
{
    ssd1306_clear(&disp);
    ssd1306_draw_empty_square(&disp, 0, 0, 127, 63);
    ssd1306_draw_string(&disp, 42, 4, 2, sys_menu_names[item]); // title
    ssd1306_draw_string(&disp, 2, 24, 1, str1);                // top line
    ssd1306_draw_string(&disp, 2, 34, 1, str2);                // second line
    ssd1306_draw_string(&disp, 7, 50, 1, str3);                // bottom line
    ssd1306_show(&disp);
}

void display_pattern_chain_edit(char *str0, char *str1, char *str2, char *str3)
{
    ssd1306_clear(&disp);
    ssd1306_draw_empty_square(&disp, 0, 0, 127, 63);
    ssd1306_draw_string(&disp, 35, 4, 2, str0);     // title
    ssd1306_draw_string(&disp, 2, 24, 1, str1);     // top line
    ssd1306_draw_string(&disp, 2, 34, 1, str2);     // second line
    ssd1306_draw_string(&disp, 7, 44, 1, str3);     // bottom line
    ssd1306_show(&disp);
}

void display_pattern_selector(uint seq, uint pattern)
{
    char strtrack[25];
    char string[6];
    sprintf(string, "%03d", pattern);
    sprintf(strtrack, "Select Trk%1d Pattern:", seq+1);
    ssd1306_clear(&disp);
    ssd1306_draw_empty_square(&disp, 0, 13, 127, 49);
    ssd1306_draw_string(&disp, 5, 20, 1, strtrack);
    ssd1306_draw_string(&disp, 50, 35, 2, string);
    ssd1306_show(&disp);
}

void display_step_selector(uint pattern, uint sel_step, char *info, char *note)
{
    char strpatt[20];
    char string[12];
    sprintf(strpatt, "Pattern:%03d", pattern);
    sprintf(string, "%02d:%s", sel_step, note);
    ssd1306_clear(&disp);
    ssd1306_draw_empty_square(&disp, 0, 0, 70, 12);
    ssd1306_draw_string(&disp, 3, 3, 1, strpatt);
    ssd1306_draw_empty_square(&disp, 0, 13, 127, 49);
    ssd1306_draw_string(&disp, 25, 20, 1, info);
    ssd1306_draw_string(&disp, 5, 35, 2, string);
    ssd1306_show(&disp);
}