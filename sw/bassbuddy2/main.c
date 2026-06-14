/**
 * @file main.c
 * 
 * Bass Buddy II (by Tommy Killander, tkilla64, MeeBilt):
 *
 * FEATURES:
 *  - CLOCK and RESET inputs 
 *  - Two tracks with separate outputs:
 *    * Track1: GATE, 1V/OCT, ACC, CV, TRIG
 *    * Track2: GATE, 1V/OCT, CV
 *  - 128x64 OLED Display
 *  - Control: 5 buttons and Encoder knob
 *  - EEPROM for calibration and parameter storage (4096 bytes)
 * 
 * IDE: Visual Studio Code with Raspberry Pi Pico SDK Extensions installed. 
 * 
 * TODO:
 *   * Add pulsewidth setting for Trig Out pulse
 *   * CV OUT LFO mode:
 *      - Tri, Saw, Random
 *      - per step or pattern repeat
 *   * Sequencer Gate control
 *   * Internal clock generation 1-255 BPM
 * 
 * BUGS: 
 *   * make sure pattern does not change during edit (turn off link)
 *   * not possible to bail out of CHAIN page with PATTERN or STEP
 *   * fix jitter in BPM calculation
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/rand.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "eeprom.h"
#include "pwm.h"
#include "switch.h"
#include "led.h"
#include "ssd1306.h"
#include "display.h"
#include "sequencer.h"

// Common definitions
#define SW_VER_MAJOR    1U      // software version 0-9
#define SW_VER_MINOR    22U     // software version 0-99
     
#define STARTUP_TIME    3000U   // ms for startup splash and LED POST cycle
#define GATE_OFF_MS     30      // Gate off time

#define SLIDE_STEP_MS   4       // time between each slide step
#define EXP_LUT_NO      20      // no of entries in Log LUT

#define MILLIVOLT_PER_NOTESTEP  83.334f // 1Volt per Octave standard

// Uncomment to get CPU load metrics for ISR and main loop
// The output will be execution time in micro-seconds
//#define MEAS_LOAD             // USB printf will affect performance
                                // so don't leave this on for production code :)

/*
 * Hardware Setup defines
 */

// Define IO pins
#define I2C_SDA1        18      // EEPROM I2C
#define I2C_SCL1        19      // EEPROM I2C
#define CLOCK_N         28      // Clock Input
#define RESET_N         29      // Reset Input

// MMI defines
const char notes[12][3] = { 
    "C-", "C#", "D-", "D#", 
    "E-", "F-", "F#", "G-", 
    "G#", "A-", "A#", "B-"
};

enum mmi_states {
    // this sysmenu items must be first and they must
    // also match with SYSMENU_ITEMS (see display.h)
    // and sys_menu_names (see display.c)
    m_sysmenu_new,          // create a new pattern
    m_sysmenu_load,         // reload pattern from EEPROM
    m_sysmenu_save,         // save pattern to EEPROM
    m_sysmenu_copy,         // copy pattern
    m_sysmenu_clock,        // trig/clock/reset settings
    m_sysmenu_seq,          // sequencer config
    m_sysmenu_cal_trk1,     // calibrate 1V/OCT output Track1
    m_sysmenu_cal_trk2,     // calibrate 1V/OCT output Track2
    m_sysmenu_tune_trk1,    // oscillator tuning Track1
    m_sysmenu_tune_trk2,    // oscillator tuning Track2
    m_sysmenu_factory_rst,  // Factory Reset
    // sysmenu stops here - see comment above

    m_main_status,          // Main Status window
    m_patt_menu_sel,        // Pattern select page
    m_step_menu_sel,        // Step select page
    m_edit_step_page,       // Edit step parameters
    m_edit_patt_link,       // Edit pattern link page
    m_edit_patt_cyc,        // Edit pattern cycles
    m_edit_patt_reset,      // Edit pattern reset
    m_sys_menu_sel,         // Sysmenu select spinner
    m_sys_cal_trk1_offs,    // calibrate offset page Track1
    m_sys_cal_trk1_gain,    // calibrate gain page Track1
    m_sys_cal_trk2_offs,    // calibrate offset page Track2
    m_sys_cal_trk2_gain,    // calibrate gain page Track2
    m_sys_tune_trk1,        // oscillator tuning page Track1
    m_sys_tune_trk2,        // oscillator tuning page Track2
    m_sys_new_ptn,          // new pattern select
    m_sys_new_steps,        // no of steps select
    m_sys_new_note,         // fill with note
    m_sys_copy_ptn,         // copy pattern select
    m_sys_copy_target,      // target pattern select
    m_sys_copy_transpose,   // transpose option
    m_sys_save_dialog,      // Save patterns page
    m_sys_load_dialog,      // Load patterns page
    m_seq_trk1_ptn,         // Track1 startup pattern
    m_seq_trk2_ptn,         // Track2 startup pattern
    m_clk_clock_pol,        // CLK polarity select
    m_clk_reset_pol,        // RESET polarity select
    m_clk_mode_sel,         // Clock mode select
    m_sys_factory_reset     // Factory reset select  
};

// Edit parameters
enum edit_params {
    e_edit_noteval,
    e_edit_tied,
    e_edit_slide,
    e_edit_trig,
    e_edit_accent,
    e_edit_cv,
    e_last_param,
};

const char edit_par[e_last_param][8] = {
    "Note",
    "Tied",
    "Slide",
    "TrigOut",
    "Accent",
    "CV Out",
};

// Slide exponential decay look-up table for simulating capacitor charge/discharge
const float exp_lut[EXP_LUT_NO] = {
    0.221f,     // 0.25t
    0.393f,     // 0.5t
    0.528f,     // 0.75t
    0.632f,     // 1t
    0.713f,     // 1.25t
    0.777f,     // 1.5t
    0.826f,     // 1.75t
    0.865f,     // 2t
    0.895f,     // 2.25t
    0.918f,     // 2.5t
    0.936f,     // 2.75t
    0.950f,     // 3t
    0.961f,     // 3.25t
    0.971f,     // 3.5t
    0.976f,     // 3.75t
    0.982f,     // 4t
    0.986f,     // 4.25t
    0.989f,     // 4.5t
    0.991f,     // 4.75t
    0.993f,     // 5t
};

// storage for calibration data and user settings
struct nonVolCal CAL; 
struct nonVolPar PAR;

// Prototypes
void compose_top_row(char *strline0);
void compose_sequencer_data(uint track, char *strline1, char*strline2);
void select_edit_parameter(uint sel_pattern, uint sel_edit_step, uint sel_param);
bool fequal(float a, float b, float epsilon);
void set_gate_output(uint track, uint value);
void set_trig_output(bool value);
uint16_t note_to_cv(uint track, uint8_t note);
void get_note_string(char *string, uint8_t note);
void goto_sys_menu(uint *mmi, uint *sysmenu);
void goto_patt_sel_menu(uint *mmi, uint8_t *sel_pattern);

/*
 * Sequencer Core
 */

uint8_t slide_step[2];      // used for slide lut index
uint16_t buff_cv[2];        // buffered CV value used to calculate slide
int32_t slide_cv[2];        // travel CV for slide to the next note
uint8_t curr_pattern[2];    // current pattern that is played
uint8_t curr_step[2];       // current step
uint8_t curr_repeat[2];     // repeat counter
uint8_t next_pattern[2];    // next pattern to play at end of pattern
uint8_t repeat_pattern[2];  // how many times to repeat the pattern
uint8_t reset_pattern[2];   // pattern to play when RESET
bool track_enable[2] =      // sequencer enable
    { false, false };
bool slide_enable[2] =      // slide to next step
    { false, false };
bool tuning_enable = false; // tuning mode

// advance sequencer to the next step 
void seq_advance_step(uint seq, bool tick)
{
    if (tick && !tuning_enable) { // tick...
        if (PAR.pattern[curr_pattern[seq]][curr_step[seq]].accent & SLIDE_MASK) {   // check is slide is activated
            slide_step[seq] = 0;
            slide_enable[seq] = true;
            slide_cv[seq] = (int32_t) (note_to_cv(seq, PAR.pattern[curr_pattern[seq]][curr_step[seq]].note & NOTE_MASK) - (int32_t) buff_cv[seq]);
        }
        else {    // Set 1V/OCT out directly if no slide
            buff_cv[seq] = note_to_cv(seq, PAR.pattern[curr_pattern[seq]][curr_step[seq]].note & NOTE_MASK);
            dual_pwm_set_out(seq, buff_cv[seq]);
        }
        // GATE_OUT = On (if note is not rest)
        if ((PAR.pattern[curr_pattern[seq]][curr_step[seq]].note & NOTE_MASK) > 0) {
            if (track_enable[seq]) {
                set_gate_output(seq, true);
                if (PAR.pattern[curr_pattern[seq]][curr_step[seq]].accent & TIED_MASK) 
                    if (seq == 0)
                        led_set_status(LED_TRACK1, red);
                    else
                        led_set_status(LED_TRACK2, green);
                else
                    if (seq == 0)
                        led_set_status(LED_TRACK1, red_fade);
                    else
                        led_set_status(LED_TRACK2, green_fade);
            }
        }
        // Set ACCENT out and TRIG out for track1 only
        if (seq == 0) {
            trk1_accent_set_out((uint8_t)(PAR.pattern[curr_pattern[seq]][curr_step[seq]].accent & ACC_MASK) * 17);
            set_trig_output(PAR.pattern[curr_pattern[seq]][curr_step[seq]].accent & TRIG_MASK);
        }
        // Set CV out
        pwm_cv_set_out(seq, PAR.pattern[curr_pattern[seq]][curr_step[seq]].vcf_cv);
    }
    else {      // ...tock
        // GATE OUT = Off (if not tied note)
        if (!(PAR.pattern[curr_pattern[seq]][curr_step[seq]].accent & TIED_MASK)) {
            set_gate_output(seq, false);
        }
        // Advance step
        curr_step[seq]++;
        // Check if at end of pattern
        if (curr_step[seq] >= (PAR.pattern_prop[curr_pattern[seq]] & PTRN_LEN_MASK)+1) {
            curr_repeat[seq]++;
            if (curr_repeat[seq] >= repeat_pattern[seq]) {
                curr_repeat[seq] = 0;
                // Link to self, just repeat pattern
                if (next_pattern[seq] == curr_pattern[seq]) {
                    next_pattern[seq] = ((PAR.pattern_prop[curr_pattern[seq]] & PTRN_CHAIN_MASK) >>5);
                }
            }
            curr_step[seq] = 0;
            curr_pattern[seq] = next_pattern[seq];
            repeat_pattern[seq] = ((PAR.pattern_prop[curr_pattern[seq]] & PTRN_REPT_MASK) >>11)+1;  
            if (((PAR.pattern_prop[curr_pattern[seq]] & PTRN_RESET_MASK) >>4))
                reset_pattern[seq] = curr_pattern[seq];
        }
    }
}

// set current pattern
void seq_set_pattern(uint seq, uint pattern)
{
    curr_pattern[seq] = pattern;
    next_pattern[seq] = pattern;
    curr_step[seq] = 0;
    curr_repeat[seq] = 0;
    repeat_pattern[seq] = ((PAR.pattern_prop[pattern] & PTRN_REPT_MASK) >>11)+1;
    if (((PAR.pattern_prop[pattern] & PTRN_RESET_MASK) >>4))
        reset_pattern[seq] = pattern;
}

// set next pattern
void seq_set_next_pattern(uint seq, uint pattern)
{
    next_pattern[seq] = pattern;
}

// calculate and set the CV output from the charge lookup table. called if slide is active
void seq_next_slide_step(uint seq)
{
    dual_pwm_set_out(seq, (uint16_t)((int32_t)buff_cv[seq] + (int32_t)((float) slide_cv[seq] * exp_lut[slide_step[seq]])));
    if (slide_step[seq] < EXP_LUT_NO-1)     // check if we reached end of table
        slide_step[seq]++;
    else {
        slide_enable[seq] = false;
        buff_cv[seq] = note_to_cv(seq, PAR.pattern[curr_pattern[seq]][curr_step[seq]].note & NOTE_MASK);
        dual_pwm_set_out(seq, note_to_cv(seq, PAR.pattern[curr_pattern[seq]][curr_step[seq]].note & NOTE_MASK));
    }
}

/*
 * ISRs
 */

#ifdef MEAS_LOAD     
volatile uint64_t timer_isr_time_us,  main_time_us; 
volatile uint64_t max_isr_time_us = 0;
volatile absolute_time_t t_entry, m_entry;
#endif

volatile uint16_t bpm_set_tempo1 = 0xFFFF;
volatile uint16_t bpm_set_tempo2 = 0xFFFF;
volatile uint16_t bpm_timer1 = 0;
volatile uint16_t bpm_timer2 = 0;
volatile uint32_t timer_counter = 0;
volatile bool ext_clock_fired1 = false;
volatile bool ext_clock_fired2 = false;
volatile bool reset_fired = false;
volatile uint8_t sel_seqencer = TRK1;
static uint64_t ext_clock_us1;
static uint64_t ext_clock_us2;
static absolute_time_t c_entry1;
static absolute_time_t c_entry2;

// Timer Interrupt Handler (1ms period time)
// Main scheduler for all concurrent activities; cycle through the LED matrix,
// poll the rotary encoder and the buttons, service slide and the sequencers.
bool on_repeating_timer_expired(struct repeating_timer *t) {
#ifdef MEAS_LOAD     
    t_entry = get_absolute_time();
#endif
    if ((timer_counter % LED_CYCLE) == 0)           // cycle through all LEDs and update states 
        led_cycle_matrix();

    if ((timer_counter % ENC_POLL) == 0)            // Readout the encoder knob value
        encoder_poll();

    if ((timer_counter % BUTTON_POLL) == 0)         // debounce all buttons and update event-mask
        buttons_poll();

    if (timer_counter % SLIDE_STEP_MS == 0) {       // check if slide is needs update
        if (slide_enable[TRK1])
            seq_next_slide_step(TRK1);

        if (slide_enable[TRK2])
            seq_next_slide_step(TRK2);
    }

    // service track 1 sequencer
    if (PAR.tempo == 0 && ext_clock_fired1) {   // external clock?
        ext_clock_fired1 = false;
        bpm_timer1 = bpm_set_tempo1;
        seq_advance_step(TRK1, true);           // tick...
    }    
    if (bpm_timer1 == GATE_OFF_MS) {
        seq_advance_step(TRK1, false);          // ...tock
    }
    bpm_timer1--;

    // service track 2 sequencer
    if (PAR.tempo == 0 && ext_clock_fired2) {   // external clock?
        ext_clock_fired2 = false;
        bpm_timer2 = bpm_set_tempo2;
        seq_advance_step(TRK2, true);           // tick...
    }    
    if (bpm_timer2 == GATE_OFF_MS) {
        seq_advance_step(TRK2, false);          // ...tock
    }
    bpm_timer2--;

    if (reset_fired) {                              // external reset
        seq_set_pattern(TRK1, reset_pattern[TRK1]);
        seq_set_pattern(TRK2, reset_pattern[TRK2]);
        reset_fired = false;
    }
    timer_counter++;
#ifdef MEAS_LOAD     
    timer_isr_time_us = absolute_time_diff_us(t_entry, get_absolute_time());
    if (timer_isr_time_us > max_isr_time_us)
        max_isr_time_us = timer_isr_time_us;
#endif
    return true;
}

// GPIO edge triggered interrupt handler
// Note that the trig GPIO inputs are inverted by HW design
void on_edge_event(uint gpio, uint32_t events) 
{
    // CLK trig depending on CLK polarity setting 
    if ((gpio == CLOCK_N && (events & GPIO_IRQ_EDGE_FALL) && !PAR.clock_pol) || 
        (gpio == CLOCK_N && (events & GPIO_IRQ_EDGE_RISE) && PAR.clock_pol)) {
        ext_clock_fired1 = true;        // clock Track1
        ext_clock_us1 = absolute_time_diff_us(c_entry1, get_absolute_time());
        c_entry1 = get_absolute_time();
        bpm_set_tempo1 = ext_clock_us1 / 1000;
        if (PAR.clock_func == 0) {      // also clock Track2 ?
            ext_clock_fired2 = true;
            ext_clock_us2 = absolute_time_diff_us(c_entry2, get_absolute_time());
            c_entry2 = get_absolute_time();
            bpm_set_tempo2 = ext_clock_us2 / 1000;
        }
    }
    // RESET trig depending on RESET polarity setting
    if ((gpio == RESET_N && (events & GPIO_IRQ_EDGE_FALL) && !PAR.reset_pol) || 
        (gpio == RESET_N && (events & GPIO_IRQ_EDGE_RISE) && PAR.reset_pol)) {       
        if (PAR.reset_func == 0)        // reset tracks ?
            reset_fired = true;
        else {                          // or clock Track2
            ext_clock_fired2 = true;
            ext_clock_us2 = absolute_time_diff_us(c_entry2, get_absolute_time());
            c_entry2 = get_absolute_time();
            bpm_set_tempo2 = ext_clock_us2 / 1000;
        }
    }
}

/*
 * MAIN SETUP:
 *
 * The setup section of main() initialises all the hardware peripherials 
 * and local variables. It reads non-volatile EEPROM areas into RAM areas
 * CAL and PAR and sets up the sequencer startup parameters.
 */

int main()
{
    char strline0[22];
    char strline1[22];
    char strline2[22];
    char strline3[22];
    char strline4[22];
    char notestring[4];

    uint mmi_state = m_main_status;         // main menu state
    uint mmi_sysmenu_item = 0;              // system menu spinner state
    uint mmi_tune_value[2] = {1,1};         // note value in TUN1 and TUN2 menues
    uint8_t sel_pattern = 0;                // latest selected pattern
    uint8_t sel_pattern2 = 0;               // latest selected pattern2
    uint sel_edit_step = 1;                 // current step in edit mode
    uint sel_param_data;                    // generic variable for parameter data
    uint sel_cycles;                        // edit cycles 
    uint step_edit_mode = e_edit_noteval;   // current step paramterer to edit
    int sel_transp = 0;                     // transpose value

    stdio_init_all();
    sleep_ms(1000);

    i2c_init(I2C_EEPROM, I2C_EE_BUSSPD);
    gpio_set_function(I2C_SDA1, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL1, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA1);
    gpio_pull_up(I2C_SCL1);
    
    display_init();
    init_button_gpios();
    init_pwms();
    led_init_gpios();

    // setup GATE and TRIG outputs
    gpio_init(TRK1_GATE);    
    gpio_set_dir(TRK1_GATE, GPIO_OUT);
    gpio_init(TRK1_TRIG);
    gpio_set_dir(TRK1_TRIG, GPIO_OUT);
    gpio_init(TRK2_GATE);    
    gpio_set_dir(TRK2_GATE, GPIO_OUT);
    // set all of them Low
    set_gate_output(TRK1, 0);
    set_gate_output(TRK2, 0);
    set_trig_output(0);

    // setup clock and reset inputs
    gpio_init(CLOCK_N);    
    gpio_set_dir(CLOCK_N, GPIO_IN);
    gpio_disable_pulls(CLOCK_N);
    gpio_init(RESET_N);    
    gpio_set_dir(RESET_N, GPIO_IN);
    gpio_disable_pulls(RESET_N);

    // setup edge triggered interrupt handlers on clock and reset inputs
    gpio_set_irq_enabled_with_callback(CLOCK_N, GPIO_IRQ_EDGE_FALL+GPIO_IRQ_EDGE_RISE, true, &on_edge_event);
    gpio_set_irq_enabled(RESET_N, GPIO_IRQ_EDGE_FALL+GPIO_IRQ_EDGE_RISE, true);

    c_entry1 = get_absolute_time();      // start value for ext clock timestamp
    c_entry2 = get_absolute_time();      // start value for ext clock timestamp
    // Setup periodic timer tick interrupt handler
    static struct repeating_timer timer;
    // Negative delay in ms means we will call repeating_timer_callback 
    // and call it again regardless of how long the callback took to execute
    add_repeating_timer_ms(-1, on_repeating_timer_expired, NULL, &timer);

    // CAL:
    // read non-volatile calibration data from EEPROM and store in RAM
    if (eeprom_read_data((uint8_t *)&CAL, sizeof(CAL), EE_CAL)) {
        eeprom_set_default_cal_data(&CAL);
    }
    else {
        if (CAL.magic != CAL_MAGIC_NO) {
            display_system_setup();
            printf("%s:Empty CAL area - Initializing...\n", __func__);
            eeprom_set_default_cal_data(&CAL);
            if (eeprom_write_data((uint8_t *)&CAL, sizeof(CAL), EE_CAL)) {
                printf("%s:EEPROM write to CAL failed!\n", __func__);
            }
            printf("%s:CAL area init completed!\n", __func__);
        }
    }

    // PAR: 
    // read non-volatile user settings from EEPROM and store in RAM
    if (eeprom_read_data((uint8_t *)&PAR, sizeof(PAR), EE_PAR)) {
        printf("%s:EEPROM read from PAR failed! - Initializing...\n", __func__);
        eeprom_set_default_par_data(&PAR);
    }
    else {
        if (PAR.magic != PAR_MAGIC_NO) {
            display_system_setup();
            printf("%s:Empty PAR area - Initializing...\n", __func__);
            eeprom_set_default_par_data(&PAR);
            // write it to EEPROM
            if (eeprom_page_write_data((uint8_t *)&PAR, sizeof(PAR), EE_PAR)) {
                printf("%s:EEPROM write to PAR failed!\n", __func__);
            }
            printf("%s:PAR area init completed!\n", __func__);
        }
    }
    sleep_ms(100);

    // Debug printout
    printf("\n\n\n*** Bass Buddy II SW Ver %d.%02d ***\n", SW_VER_MAJOR, SW_VER_MINOR);
    printf("EE size: %4d bytes, EE used: %4d bytes\n\n",eeprom_size_probe() , sizeof(CAL)+sizeof(PAR));
    printf("Track1 start pattern: %03d  Track2 start pattern: %03d\n", PAR.seq_ptn_start[TRK1], PAR.seq_ptn_start[TRK2]);
    printf("Clock In polarity: %s    Clock In Function: %s\n", PAR.clock_pol ? "Fall" : "Rise", (PAR.clock_func == 0) ? "Clock 1&2" : "Clock 1");
    printf("Reset In polarity: %s    Reset In Function: %s\n", PAR.reset_pol ? "Fall" : "Rise", (PAR.reset_func == 0) ? "Reset" : "Clock 2");
    printf("\nOccupied patterns [--]=Unused [nn]=Occupied(steps):\n");
    for (int ptn=0 ; ptn < MAXPATTERN ; ptn++) {
        int len = (PAR.pattern_prop[ptn] & PTRN_LEN_MASK)+1;
        int notesum = 0;
        for (int stp=0 ; stp < len ; stp++) {
            notesum += PAR.pattern[ptn][stp].note;
        }
        if ((ptn % 10) == 0)
            printf("\n");

        if (notesum)
            printf("[%02d]", len);
        else
            printf("[--]");
    }
    printf("\n\n");

    //
    // Setup sequencers with startup parameters
    if (PAR.tempo > 0) {
        bpm_set_tempo1 = (1000 / ((PAR.tempo * 4) / 60));    
    }
    track_enable[TRK1] = true;
    seq_set_pattern(TRK1, PAR.seq_ptn_start[TRK1]);
    track_enable[TRK2] = true;
    seq_set_pattern(TRK2, PAR.seq_ptn_start[TRK2]);
      
    //
    // startup splash screen; show firmware version
    display_version(SW_VER_MAJOR, SW_VER_MINOR);
    led_post_cycle(STARTUP_TIME);

    /*
     * MAIN LOOP:
     *
     * The main loop takes care of the user interface; updating the main screen, reading
     * buttons and the encoder values and navigating through the menu system. The menu 
     * system is implemented as a finite state machine (mmi_state), keeping track of what
     * to show on the display and what the buttons and encoder do.
     */
    while (true) {
#ifdef MEAS_LOAD        
        m_entry = get_absolute_time();
#endif
        //
        // Handle MMI
        switch (mmi_state) {
            // Main status page
            case m_main_status:
                compose_top_row(strline0);
                compose_sequencer_data(TRK1, strline1, strline2);
                compose_sequencer_data(TRK2, strline3, strline4);
                display_main_window((PAR.clock_func == 0), track_enable[TRK1], track_enable[TRK2], sel_seqencer, strline0, strline1, strline2, strline3, strline4);
                if (read_menu_button()) {   // system menu
                    goto_sys_menu(&mmi_state, &mmi_sysmenu_item);
                    clear_button_event(ALL_EV);
                }
                if (read_pattern_button()) {    // select pattern
                    goto_patt_sel_menu(&mmi_state, &sel_pattern);
                    clear_button_event(ALL_EV);
                }
                if (read_step_button()) {   // step edit
                    mmi_state = m_step_menu_sel;
                    clear_button_event(ALL_EV);
                    sel_pattern = curr_pattern[sel_seqencer];                    
                    encoder_set_data(sel_edit_step, 1, (PAR.pattern_prop[sel_pattern] & PTRN_LEN_MASK)+1);
                    led_set_status(LED_STEP, green);
                }
                if (read_track_button()) {  // toggle selected track
                    sel_seqencer = (sel_seqencer == TRK1) ? TRK2 : TRK1;
                    clear_button_event(ALL_EV);
                }
                if (read_enc_sw_button_status()) {  // arm EDIT button for Track Enable
                    led_set_status(LED_EDIT, green);
                    if (read_edit_button()) {       // toggle Track Enable mode
                        track_enable[sel_seqencer] = (track_enable[sel_seqencer]) ? false : true;
                    }
                }
                else {  // disarm EDIT button
                    led_set_status(LED_EDIT, off);
                    clear_button_event(EDIT_EV);
                }
                break;

            // Pattern select page
            case m_patt_menu_sel:
                led_set_status(LED_EDIT, green);
                sel_pattern = encoder_get_data();
                display_pattern_selector(sel_seqencer, sel_pattern);
                if (read_pattern_button()) {    // go back to main page
                    mmi_state = m_main_status;
                    clear_button_event(ALL_EV);
                    led_set_status(LED_PATTERN, off);
                    led_set_status(LED_EDIT, off);
                }
                if (read_enc_sw_button()) {     // set pattern
                    mmi_state = m_main_status;
                    clear_button_event(ALL_EV);
                    led_set_status(LED_PATTERN, off);
                    led_set_status(LED_EDIT, off);
                    seq_set_next_pattern(sel_seqencer, sel_pattern);
                }
                if (read_track_button()) {      // track select toggle
                    sel_seqencer = (sel_seqencer == TRK1) ? TRK2 : TRK1;
                    sel_pattern = curr_pattern[sel_seqencer];
                    encoder_set_data(sel_pattern, 0, MAXPATTERN-1);
                    clear_button_event(ALL_EV);
                }
                if (read_edit_button()) {       // edit pattern CHAIN
                    mmi_state = m_edit_patt_link;
                    encoder_set_data(((PAR.pattern_prop[sel_pattern] & PTRN_CHAIN_MASK) >>5), 0, MAXPATTERN-1);
                    clear_button_event(ALL_EV);
                    led_set_status(LED_PATTERN, red);
                }
                if (read_step_button()) {       // step edit
                    mmi_state = m_step_menu_sel;
                    clear_button_event(ALL_EV);
                    sel_pattern = curr_pattern[sel_seqencer];                    
                    encoder_set_data(sel_edit_step, 1, (PAR.pattern_prop[sel_pattern] & PTRN_LEN_MASK)+1);
                    led_set_status(LED_STEP, green);
                    led_set_status(LED_PATTERN, off);
                    led_set_status(LED_EDIT, off);
                }
                break;

            // Edit pattern LINK, CYCLE and RESET
            case m_edit_patt_link:
                sel_pattern2 = encoder_get_data();
                sprintf(strline0, "   Link %03d to %03d", sel_pattern, sel_pattern2);
                display_pattern_chain_edit("CHAIN", strline0, "", "");
                if (read_edit_button()) {       // go back to patt sel
                    goto_patt_sel_menu(&mmi_state, &sel_pattern);
                }
                if (read_enc_sw_button()) {     // set link
                    PAR.pattern_prop[sel_pattern] = (PAR.pattern_prop[sel_pattern] & ~PTRN_CHAIN_MASK) | sel_pattern2 <<5;
                    mmi_state = m_edit_patt_cyc;
                    clear_button_event(ALL_EV);
                    encoder_set_data(((PAR.pattern_prop[sel_pattern] & PTRN_REPT_MASK) >>11)+1, 1, MAXCYCLES);
                }
                break;

            case m_edit_patt_cyc:
                sel_cycles = encoder_get_data();
                sprintf(strline1, "   No of cycles %2d", sel_cycles);
                display_pattern_chain_edit("CHAIN", strline0, strline1, "");
                if (read_edit_button()) {       // go back to patt sel
                    goto_patt_sel_menu(&mmi_state, &sel_pattern);
                }                
                if (read_enc_sw_button()) {     // set cycles
                    PAR.pattern_prop[sel_pattern] = (PAR.pattern_prop[sel_pattern] & ~PTRN_REPT_MASK) | sel_cycles-1 <<11;
                    mmi_state = m_edit_patt_reset;
                    clear_button_event(ALL_EV);
                    encoder_set_data(((PAR.pattern_prop[sel_pattern] & PTRN_RESET_MASK) >>4), 0, 1);
                }
                break;

            case m_edit_patt_reset:
                sel_param_data = encoder_get_data();
                sprintf(strline2, "  Reset to here? %c", sel_param_data ? 'Y' : 'N');
                display_pattern_chain_edit("CHAIN", strline0, strline1, strline2);            
                if (read_edit_button()) {       // go back to patt sel
                    goto_patt_sel_menu(&mmi_state, &sel_pattern);
                }                
                if (read_enc_sw_button()) {     // set reset waypoint
                    PAR.pattern_prop[sel_pattern] = (PAR.pattern_prop[sel_pattern] & ~PTRN_RESET_MASK) | sel_param_data <<4;
                    goto_patt_sel_menu(&mmi_state, &sel_pattern);
                }
                break;

            // Step select page
            case m_step_menu_sel:
                sel_edit_step = encoder_get_data();
                get_note_string(notestring, PAR.pattern[sel_pattern][sel_edit_step-1].note & NOTE_MASK);
                sprintf(strline0, "%s %c%c%c", notestring, 
                    (PAR.pattern[sel_pattern][sel_edit_step-1].accent & TIED_MASK) ? 'T' : '-',
                    (PAR.pattern[sel_pattern][sel_edit_step-1].accent & SLIDE_MASK) ? 'S' : '-',
                    (PAR.pattern[sel_pattern][sel_edit_step-1].accent & TRIG_MASK) ? 'P' : '-' 
                );
                display_step_selector(sel_pattern, sel_edit_step, "Select Step:", strline0);
                if (read_step_button()) {   // go back to main page
                    mmi_state = m_main_status;
                    clear_button_event(ALL_EV);
                    led_set_status(LED_STEP, off);
                }
                if (read_enc_sw_button()) { // enter edit parameter menu
                    mmi_state = m_edit_step_page;
                    select_edit_parameter(sel_pattern, sel_edit_step, step_edit_mode);
                    clear_button_event(ALL_EV);
                    led_set_status(LED_STEP, red);
                    led_set_status(LED_EDIT, green);
                }
                if (read_pattern_button()) { // select pattern
                    goto_patt_sel_menu(&mmi_state, &sel_pattern);
                    clear_button_event(ALL_EV);
                    led_set_status(LED_STEP, off);
                }
                break;
                
            // Edit step parameter values
            case m_edit_step_page:
                sel_param_data = encoder_get_data();
                switch (step_edit_mode) {   // update the selected param data
                    case e_edit_noteval:
                        PAR.pattern[sel_pattern][sel_edit_step-1].note = sel_param_data;
                        break;
                    case e_edit_tied:
                        if (sel_param_data == 1)
                            PAR.pattern[sel_pattern][sel_edit_step-1].accent |= TIED_MASK;
                        else
                            PAR.pattern[sel_pattern][sel_edit_step-1].accent &= ~TIED_MASK;
                        break;
                    case e_edit_slide:
                        if (sel_param_data == 1)
                            PAR.pattern[sel_pattern][sel_edit_step-1].accent |= SLIDE_MASK;
                        else
                            PAR.pattern[sel_pattern][sel_edit_step-1].accent &= ~SLIDE_MASK;
                        break;
                    case e_edit_trig:
                        if (sel_param_data == 1)
                            PAR.pattern[sel_pattern][sel_edit_step-1].accent |= TRIG_MASK;
                        else
                            PAR.pattern[sel_pattern][sel_edit_step-1].accent &= ~TRIG_MASK;
                        break;
                    case e_edit_accent:
                        PAR.pattern[sel_pattern][sel_edit_step-1].accent = sel_param_data;
                        break;
                    case e_edit_cv:
                        PAR.pattern[sel_pattern][sel_edit_step-1].vcf_cv = sel_param_data;
                        break;
                }
                get_note_string(notestring, PAR.pattern[sel_pattern][sel_edit_step-1].note & NOTE_MASK);
                switch (step_edit_mode) {   // set page layout according to which param we are editing
                    case e_edit_noteval:
                    case e_edit_tied:
                    case e_edit_slide:
                    case e_edit_trig:
                        sprintf(strline0, "%s %c%c%c", notestring, 
                            (PAR.pattern[sel_pattern][sel_edit_step-1].accent & TIED_MASK) ? 'T' : '-',
                            (PAR.pattern[sel_pattern][sel_edit_step-1].accent & SLIDE_MASK) ? 'S' : '-', 
                            (PAR.pattern[sel_pattern][sel_edit_step-1].accent & TRIG_MASK) ? 'P' : '-');
                        break;
                    case e_edit_accent:
                        sprintf(strline0, "%s %c%02d", notestring, 
                            (PAR.pattern[sel_pattern][sel_edit_step-1].accent & ACC_MASK) ? 'A' : '-',
                            (PAR.pattern[sel_pattern][sel_edit_step-1].accent & ACC_MASK));
                        break;
                    case e_edit_cv:
                        sprintf(strline0, "%s %03d", notestring, 
                            PAR.pattern[sel_pattern][sel_edit_step-1].vcf_cv & VCF_CV_MASK);
                        break;
                }
                sprintf(strline1, "Edit %s", edit_par[step_edit_mode]);
                display_step_selector(sel_pattern, sel_edit_step, strline1, strline0);
                if (read_step_button()) {   // go back to step sel page
                    mmi_state = m_step_menu_sel;
                    encoder_set_data(sel_edit_step, 1, (PAR.pattern_prop[sel_pattern] & PTRN_LEN_MASK)+1);
                    clear_button_event(ALL_EV);
                    led_set_status(LED_STEP, green);
                    led_set_status(LED_EDIT, off);
                }
                if (read_edit_button()) {   // cycle through parameters
                    clear_button_event(ALL_EV);
                    if (++step_edit_mode == e_last_param)
                        step_edit_mode = e_edit_noteval;
                    select_edit_parameter(sel_pattern, sel_edit_step, step_edit_mode);
                }
                if (read_enc_sw_button()) { // advance to next step (with wraparound)
                    clear_button_event(ALL_EV);
                    if (++sel_edit_step > (PAR.pattern_prop[sel_pattern] & PTRN_LEN_MASK)+1)
                        sel_edit_step = 1;
                    select_edit_parameter(sel_pattern, sel_edit_step, step_edit_mode);
                }
                break;

            // System Menu spinner page
            case m_sys_menu_sel:
                mmi_sysmenu_item = encoder_get_data()%SYSMENU_ITEMS;
                display_sysmenu_spinner_window(mmi_sysmenu_item, 0);
                if (read_menu_button()) {       // go back
                    mmi_state = m_main_status;
                    clear_button_event(ALL_EV);
                }
                if (read_enc_sw_button()) {     // jump to the system sub-menu
                    switch (mmi_sysmenu_item) {
                        case m_sysmenu_new:
                            encoder_set_data(sel_pattern, 0, MAXPATTERN-1);
                            clear_button_event(MENU_EV);
                            mmi_state = m_sys_new_ptn;
                            break;

                        case m_sysmenu_copy:
                            encoder_set_data(sel_pattern, 0, MAXPATTERN-1);
                            clear_button_event(MENU_EV);
                            mmi_state = m_sys_copy_ptn;
                            break;

                        case m_sysmenu_load:
                            display_pattern_window(m_sysmenu_load, " Load ALL patterns?", "", "Press knob to load");
                            clear_button_event(MENU_EV);
                            mmi_state = m_sys_load_dialog;
                            break;

                        case m_sysmenu_save:
                            display_pattern_window(m_sysmenu_save, " Save ALL patterns?", "", "Press knob to save");
                            clear_button_event(MENU_EV);
                            mmi_state = m_sys_save_dialog;
                            break;

                        case m_sysmenu_cal_trk1:
                            mmi_state = m_sys_cal_trk1_offs;
                            tuning_enable = true;
                            encoder_set_data(CAL.offs_100mv[TRK1], -100, 100);
                            clear_button_event(MENU_EV);
                            break;

                        case m_sysmenu_cal_trk2:
                            mmi_state = m_sys_cal_trk2_offs;
                            tuning_enable = true;
                            encoder_set_data(CAL.offs_100mv[TRK2], -100, 100);                
                            clear_button_event(MENU_EV);
                            break;

                        case m_sysmenu_tune_trk1:
                            mmi_state = m_sys_tune_trk1;
                            tuning_enable = true;
                            encoder_set_data(mmi_tune_value[TRK1], 1, 60);
                            clear_button_event(MENU_EV|TRACK_EV);
                            led_set_status(LED_TRACK1, red);
                            led_set_status(LED_TRACK2, off);
                            break;

                        case m_sysmenu_tune_trk2:
                            mmi_state = m_sys_tune_trk2;
                            tuning_enable = true;
                            encoder_set_data(mmi_tune_value[TRK2], 1 ,60);                
                            clear_button_event(MENU_EV|TRACK_EV);
                            led_set_status(LED_TRACK2, green);
                            led_set_status(LED_TRACK1, off);
                            break;

                        case m_sysmenu_seq:
                            encoder_set_data(PAR.seq_ptn_start[TRK1], 0, MAXPATTERN-1);
                            clear_button_event(MENU_EV);
                            mmi_state = m_seq_trk1_ptn;
                            break;

                        case m_sysmenu_clock:
                            encoder_set_data(2*(int)PAR.clock_pol, 0, 3);
                            clear_button_event(MENU_EV);
                            mmi_state = m_clk_clock_pol;
                            break;

                        case m_sysmenu_factory_rst:
                            if (read_edit_button_status()) {    // hidden function
                                // print pattern steps
                                printf("\n\nconst seq_step_t pres_ptn[PRESETS][MAXSTEPS] = {\n");
                                for (int ptn=0 ; ptn < PRESETS ; ptn++) {
                                    printf("  {\n");
                                    for (int stp=0 ; stp < MAXSTEPS ; stp++) {
                                        printf("    0x%02x, 0x%02x, 0x%02x, 0x%02x, \n", 
                                            PAR.pattern[ptn][stp].note, PAR.pattern[ptn][stp].gate,
                                            PAR.pattern[ptn][stp].accent, PAR.pattern[ptn][stp].vcf_cv);
                                    }
                                    printf("  },\n\n");
                                }
                                printf("};\n");
                                // print pattern properties
                                printf("\n\nconst uint16_t pres_prop[PRESETS] = {\n");
                                for (int ptn=0 ; ptn < PRESETS ; ptn++) {
                                        printf("    0x%04x, ", 
                                            PAR.pattern_prop[ptn]);
                                }
                                printf("\n};\n");
                            }
                            sprintf(strline0, " Clear ALL Settings");
                            encoder_set_data(0, 0, 3);
                            clear_button_event(MENU_EV);
                            mmi_state = m_sys_factory_reset;
                            break;

                        default:
                            mmi_state = m_main_status;
                            break;
                    }
                }
                break;

            case m_seq_trk1_ptn:
                sel_pattern = encoder_get_data();
                sprintf(strline0, " Track1 Pattern: %03d", sel_pattern);
                display_pattern_window(m_sysmenu_seq, strline0, "", "Set startup pattern");
                if (read_menu_button()) {   // bail out
                    goto_sys_menu(&mmi_state, &mmi_sysmenu_item);
                }
                if (read_enc_sw_button()) {  // track1 startup pattern
                    PAR.seq_ptn_start[TRK1] = sel_pattern;
                    encoder_set_data(PAR.seq_ptn_start[TRK2], 0, MAXPATTERN-1);
                    mmi_state = m_seq_trk2_ptn;
                    clear_button_event(ALL_EV);
                }
                break;

            case m_seq_trk2_ptn:
                sel_pattern = encoder_get_data();
                sprintf(strline0, " Track2 Pattern: %03d", sel_pattern);
                display_pattern_window(m_sysmenu_seq, strline0, "", "Press knob to save");
                if (read_menu_button()) {   // bail out
                    goto_sys_menu(&mmi_state, &mmi_sysmenu_item);
                }
                if (read_enc_sw_button()) {  // track2 startup pattern
                    PAR.seq_ptn_start[TRK2] = sel_pattern;
                    // Save the parameters to EEPROM
                    if (eeprom_write_data((uint8_t *)&PAR.seq_ptn_start, sizeof(PAR.seq_ptn_start), 
                        EE_PAR+((uint32_t)&PAR.seq_ptn_start[0] - (uint32_t)&PAR.pattern[0][0]))) {
                        printf("%s:EEPROM write to PAR failed!\n", __func__);
                    }
                    goto_sys_menu(&mmi_state, &mmi_sysmenu_item);
                }
                break;

            case m_clk_clock_pol:
                sel_param_data = encoder_get_data();
                sprintf(strline0, " CLOCK Polarity:%s", (sel_param_data > 1) ? "Fall" : "Rise");
                display_pattern_window(m_sysmenu_clock, strline0, "", "");
                if (read_menu_button()) {   // bail out
                    goto_sys_menu(&mmi_state, &mmi_sysmenu_item);
                }
                if (read_enc_sw_button()) {  // set Clock In polarity
                    PAR.clock_pol = (sel_param_data > 1);
                    encoder_set_data(2*(int)PAR.reset_pol, 0, 3);
                    mmi_state = m_clk_reset_pol;
                    clear_button_event(ALL_EV);
                }            
                break;

            case m_clk_reset_pol:
                sel_param_data = encoder_get_data();
                sprintf(strline1, " RESET Polarity:%s", (sel_param_data > 1) ? "Fall" : "Rise");
                display_pattern_window(m_sysmenu_clock, strline0, strline1, "");
                if (read_menu_button()) {   // bail out
                    goto_sys_menu(&mmi_state, &mmi_sysmenu_item);
                }
                if (read_enc_sw_button()) {  // set Reset In polarity
                    PAR.reset_pol = (sel_param_data > 1);
                    encoder_set_data(2*PAR.clock_func, 0, 3);
                    mmi_state = m_clk_mode_sel;
                    clear_button_event(ALL_EV);
                }            
                break;

            case m_clk_mode_sel:
                sel_param_data = encoder_get_data();
                sprintf(strline2, " CLK:%s RES:%s", (sel_param_data > 1) ? "Tr1  " : "Tr1&2", (sel_param_data > 1) ? "Tr2" : "Res");
                display_pattern_window(m_sysmenu_clock, strline0, strline1, strline2);
                if (read_menu_button()) {   // bail out
                    goto_sys_menu(&mmi_state, &mmi_sysmenu_item);
                }
                if (read_enc_sw_button()) {  // set CLK and RES mode
                    PAR.clock_func = (sel_param_data > 1) ? 1 : 0;
                    PAR.reset_func = (sel_param_data > 1) ? 1 : 0;
                    // Save clock parameters in EEPROM
                    if (eeprom_write_data((uint8_t *)&PAR.clock_pol, (sizeof(PAR.clock_pol)+sizeof(PAR.reset_pol)+sizeof(PAR.clock_func)+sizeof(PAR.reset_func)), 
                        EE_PAR+((uint32_t)&PAR.clock_pol - (uint32_t)&PAR.pattern[0][0]))) {
                        printf("%s:EEPROM write to PAR failed!\n", __func__);
                    }
                    goto_sys_menu(&mmi_state, &mmi_sysmenu_item);
                }            
                break;

            case m_sys_new_ptn:
                sel_pattern = encoder_get_data();
                sprintf(strline0, " Select pattern: %03d", sel_pattern);
                display_pattern_window(m_sysmenu_new, strline0, "", "");
                if (read_menu_button()) {   // bail out
                    goto_sys_menu(&mmi_state, &mmi_sysmenu_item);
                }
                if (read_enc_sw_button()) {  // next parameter (steps)
                    encoder_set_data(sel_edit_step, 1, MAXSTEPS);
                    mmi_state = m_sys_new_steps;
                    clear_button_event(ALL_EV);
                }
                break;

            case m_sys_new_steps:
                sel_edit_step = encoder_get_data();
                sprintf(strline1, "  No. of Steps: %02d", sel_edit_step);
                display_pattern_window(m_sysmenu_new, strline0, strline1, "");
                if (read_menu_button()) {   // bail out
                    goto_sys_menu(&mmi_state, &mmi_sysmenu_item);
                }
                if (read_enc_sw_button()) {  // next parameter (note val)
                    encoder_set_data(sel_param_data, 0, 60);
                    mmi_state = m_sys_new_note;
                    clear_button_event(ALL_EV);
                }
                break;

            case m_sys_new_note:
                sel_param_data = encoder_get_data();
                get_note_string(notestring, sel_param_data);
                sprintf(strline2, " Set note val: %s", notestring);
                display_pattern_window(m_sysmenu_new, strline0, strline1, strline2);
                if (read_menu_button()) {   // bail out
                    goto_sys_menu(&mmi_state, &mmi_sysmenu_item);
                }
                if (read_enc_sw_button()) {  // create the actual pattern
                    for (int stp=0 ; stp < sel_edit_step ; stp++) {
                        PAR.pattern[sel_pattern][stp].accent = 0;               // accent off
                        PAR.pattern[sel_pattern][stp].note = sel_param_data;    // init note value
                        PAR.pattern[sel_pattern][stp].gate = 0;                 // gate params
                        PAR.pattern[sel_pattern][stp].vcf_cv = 0;               // VCF CV param
                    }
                    PAR.pattern_prop[sel_pattern] = 
                        (sel_edit_step-1 & PTRN_LEN_MASK) |                     // pattern length 
                        (sel_pattern<<5);                                       // repeat same ptn

                    PAR.pattern_prop[sel_pattern] = 
                        PAR.pattern_prop[sel_pattern] | PTRN_RESET_MASK;        // Set RESET bit

                    goto_sys_menu(&mmi_state, &mmi_sysmenu_item);
                }
                break;

            case m_sys_copy_ptn:
                sel_pattern = encoder_get_data();
                sprintf(strline0, " Source pattern: %03d", sel_pattern);
                display_pattern_window(m_sysmenu_copy, strline0, "", "");
                if (read_menu_button()) {   // bail out
                    goto_sys_menu(&mmi_state, &mmi_sysmenu_item);
                }
                if (read_enc_sw_button()) {  // target pattern
                    encoder_set_data(sel_pattern, 0, MAXPATTERN-1);
                    mmi_state = m_sys_copy_target;
                    clear_button_event(ALL_EV);
                }
                break;

            case m_sys_copy_target:
                sel_pattern2 = encoder_get_data();
                sprintf(strline1, " Target pattern: %03d", sel_pattern2);
                display_pattern_window(m_sysmenu_copy, strline0, strline1, "");
                if (read_menu_button()) {   // bail out
                    goto_sys_menu(&mmi_state, &mmi_sysmenu_item);
                }
                if (read_enc_sw_button()) {  // transpose option
                    encoder_set_data(sel_transp, -12, 12);
                    mmi_state = m_sys_copy_transpose;
                    clear_button_event(ALL_EV);
                }            
                break;

            case m_sys_copy_transpose:
                sel_transp = encoder_get_data();
                sprintf(strline2, "   Transpose: %2d", sel_transp);
                display_pattern_window(m_sysmenu_copy, strline0, strline1, strline2);
                if (read_menu_button()) {   // bail out
                    goto_sys_menu(&mmi_state, &mmi_sysmenu_item);
                }
                if (read_enc_sw_button()) {  // copy pattern and transpose
                    for (int step=0 ; step < (int)(PAR.pattern_prop[sel_pattern] & PTRN_LEN_MASK)+1 ; step++) {
                        // handle note transpose
                        int note = PAR.pattern[sel_pattern][step].note + sel_transp;
                        if (PAR.pattern[sel_pattern][step].note == 0) // if a rest, keep it
                            PAR.pattern[sel_pattern2][step].note = 0;
                        else if (note < 1)  // less than first note, add one octave
                            PAR.pattern[sel_pattern2][step].note = note + 12;
                        else if (note > 60) // more than max note, subtract one octave
                            PAR.pattern[sel_pattern2][step].note = note - 12;
                        else
                            PAR.pattern[sel_pattern2][step].note = note;
                        // copy reset of step parameters
                        PAR.pattern[sel_pattern2][step].accent = PAR.pattern[sel_pattern][step].accent; 
                        PAR.pattern[sel_pattern2][step].gate = PAR.pattern[sel_pattern][step].gate;
                        PAR.pattern[sel_pattern2][step].vcf_cv = PAR.pattern[sel_pattern][step].vcf_cv;
                    }
                    // copy pattern length and set new repeat pattern
                    PAR.pattern_prop[sel_pattern2] = PAR.pattern_prop[sel_pattern] & PTRN_LEN_MASK
                                                  | (sel_pattern2<<5);
                    // Clear RESET bit
                    PAR.pattern_prop[sel_pattern2] = PAR.pattern_prop[sel_pattern2] & ~PTRN_RESET_MASK;        
                    goto_sys_menu(&mmi_state, &mmi_sysmenu_item);
                }
                break;

            case m_sys_save_dialog:
                if (read_menu_button()) {   // bail out
                    goto_sys_menu(&mmi_state, &mmi_sysmenu_item);
                }
                if (read_enc_sw_button()) { // save patterns
                    display_pattern_window(m_sysmenu_save, "    Saving data...", "", "");
                    if (eeprom_page_write_data((uint8_t *)&PAR, sizeof(PAR), EE_PAR)) {
                        printf("%s:EEPROM write to PAR failed!\n", __func__);
                    }
                    mmi_state = m_main_status;
                    clear_button_event(ALL_EV);
                }
                break;

            case m_sys_load_dialog:
                if (read_menu_button()) {   // bail out
                    goto_sys_menu(&mmi_state, &mmi_sysmenu_item);
                }
                if (read_enc_sw_button()) { // save patterns
                    display_pattern_window(m_sysmenu_load, "    Reading data...", "", "");
                    sleep_ms(500);
                    if (eeprom_read_data((uint8_t *)&PAR, sizeof(PAR), EE_PAR)) {
                        printf("%s:EEPROM read from PAR failed!\n", __func__);
                    }
                    mmi_state = m_main_status;
                    clear_button_event(ALL_EV);
                }
                break;

            case m_sys_cal_trk1_offs:
                // set the output to 50mV
                // calibrate offset to match
                dual_pwm_set_out(TRK1, constrain((50.0f * ((float)CAL.mstep_mv[TRK1] / 1000.0f)) + CAL.offs_100mv[TRK1], 0, 0xFFFF));
                CAL.offs_100mv[TRK1] = (uint16_t)encoder_get_data();
                sprintf(strline1, " Offs:%2d\n", CAL.offs_100mv[TRK1]);
                display_cal_window(m_sysmenu_cal_trk1, " TRK1 1V/OCT->50mV", strline1);
                if (read_menu_button()) {   // bail out
                    mmi_state = m_main_status;
                    clear_button_event(ALL_EV);
                    tuning_enable = false;
                }
                if (read_enc_sw_button()) { // set offset and move to next
                    mmi_state = m_sys_cal_trk1_gain;
                    encoder_set_data(CAL.mstep_mv[TRK1], 12000, 14000);                    
                }
                break;

            case m_sys_cal_trk1_gain:
                // set the output to 4750 mV
                // calibrate gain to match
                dual_pwm_set_out(TRK1, note_to_cv(TRK1, 58));
                CAL.mstep_mv[TRK1] = (uint16_t)encoder_get_data();
                sprintf(strline1, "Gain:%2d\n", CAL.mstep_mv[TRK1]);
                display_cal_window(m_sysmenu_cal_trk1, " TRK1 1V/OCT->4750mV", strline1);
                if (read_menu_button()) {   // bail out
                    mmi_state = m_main_status;
                    clear_button_event(ALL_EV);
                    tuning_enable = false;
                }
                if (read_enc_sw_button()) { // set gain and save CAL
                    display_cal_window(m_sysmenu_cal_trk1, "    Saving data...", "");
                    sleep_ms(500);
                    if (eeprom_write_data((uint8_t *)&CAL, sizeof(CAL), EE_CAL)) {
                        printf("%s:EEPROM write to CAL failed!\n", __func__);
                    }
                    clear_button_event(ALL_EV);
                    mmi_state = m_main_status;
                    tuning_enable = false;
                }
                break;

            case m_sys_cal_trk2_offs:
                // set the output to 50mV
                // calibrate offset to match
                dual_pwm_set_out(TRK2, constrain((50.0f * ((float)CAL.mstep_mv[TRK2] / 1000.0f)) + CAL.offs_100mv[TRK2], 0, 0xFFFF));
                CAL.offs_100mv[TRK2] = (uint16_t)encoder_get_data();
                sprintf(strline1, " Offs:%2d\n", CAL.offs_100mv[TRK2]);
                display_cal_window(m_sysmenu_cal_trk2, " TRK2 1V/OCT->50mV", strline1);
                if (read_menu_button()) {   // bail out
                    mmi_state = m_main_status;
                    tuning_enable = false;
                }
                if (read_enc_sw_button()) { // set offset and move to next
                    mmi_state = m_sys_cal_trk2_gain;
                    encoder_set_data(CAL.mstep_mv[TRK2], 12000, 14000);                    
                }
                break;

            case m_sys_cal_trk2_gain:
                // set the output to 4750 mV
                // calibrate gain to match
                dual_pwm_set_out(TRK2, note_to_cv(TRK2, 58));
                CAL.mstep_mv[TRK2] = (uint16_t)encoder_get_data();
                sprintf(strline1, "Gain:%2d\n", CAL.mstep_mv[TRK2]);
                display_cal_window(m_sysmenu_cal_trk2, " TRK2 1V/OCT->4750mV", strline1);
                if (read_menu_button()) {   // bail out
                    mmi_state = m_main_status;
                    tuning_enable = false;
                }
                if (read_enc_sw_button()) { // set gain and save CAL
                    display_cal_window(m_sysmenu_cal_trk2, "    Saving data...", "");
                    sleep_ms(500);
                    if (eeprom_write_data((uint8_t *)&CAL, sizeof(CAL), EE_CAL)) {
                        printf("%s:EEPROM write to CAL failed!\n", __func__);
                    }
                    mmi_state = m_main_status;
                    tuning_enable = false;
                }
                break;

            case m_sys_tune_trk1:
                // output a note value on 1V/Oct Track1
                // this is useful for oscillator tuning
                mmi_tune_value[TRK1] = encoder_get_data();
                dual_pwm_set_out(TRK1, note_to_cv(TRK1, mmi_tune_value[TRK1]));
                get_note_string(strline0, mmi_tune_value[TRK1]);
                display_tune_window(m_sysmenu_tune_trk1, strline0 , "Select note w. knob");
                if (read_menu_button()) {   // bail out
                    mmi_state = m_main_status;
                    tuning_enable = false;
                    led_set_status(LED_TRACK1, off);
                }
                if (read_track_button()) {   // jump to CAL2
                    mmi_state = m_sys_tune_trk2;
                    encoder_set_data(mmi_tune_value[TRK2], 1, 60);
                    tuning_enable = true;
                    led_set_status(LED_TRACK2, green);
                    led_set_status(LED_TRACK1, off);
                }
                break;

            case m_sys_tune_trk2:
                // output a note value on 1V/Oct Track2
                // this is useful for oscillator tuning
                mmi_tune_value[TRK2] = encoder_get_data();
                dual_pwm_set_out(TRK2, note_to_cv(TRK2, mmi_tune_value[TRK2]));
                get_note_string(strline0, mmi_tune_value[TRK2]);
                display_tune_window(m_sysmenu_tune_trk2, strline0 , "Select note w. knob");
                if (read_menu_button()) {   // bail out
                    mmi_state = m_main_status;
                    tuning_enable = false;
                    led_set_status(LED_TRACK2, off);
                }
                if (read_track_button()) {   // jump to CAL1
                    mmi_state = m_sys_tune_trk1;
                    encoder_set_data(mmi_tune_value[TRK1], 1, 60);
                    tuning_enable = true;
                    led_set_status(LED_TRACK1, red);
                    led_set_status(LED_TRACK2, off);
                }
                break;

            case m_sys_factory_reset:
                sel_param_data = encoder_get_data();
                sprintf(strline1, "Restore EEPROM?:%s", (sel_param_data > 1) ? "YES" : "NO");
                display_pattern_window(m_sysmenu_factory_rst, strline0, "", strline1);
                if (read_menu_button()) {   // bail out
                    goto_sys_menu(&mmi_state, &mmi_sysmenu_item);
                }
                if (read_enc_sw_button()) { 
                    if (sel_param_data > 1) {   // restore all parameters and patterns
                        display_system_setup();
                        printf("%s:Empty PAR area - Initializing...\n", __func__);
                        eeprom_set_default_par_data(&PAR);
                        // write it to EEPROM
                        if (eeprom_page_write_data((uint8_t *)&PAR, sizeof(PAR), EE_PAR)) {
                            printf("%s:EEPROM write to PAR failed!\n", __func__);
                        }
                        printf("%s:PAR area init completed!\n", __func__);
                    }
                    clear_button_event(ALL_EV);
                    mmi_state = m_main_status;
                }
                break;
                
            default:
                break;
        } /* switch */
#ifdef MEAS_LOAD     
        main_time_us = absolute_time_diff_us(m_entry, get_absolute_time());
        printf("Timer ISR %llu/(max %llu) us , Main %llu us\n", timer_isr_time_us, max_isr_time_us, main_time_us);
#endif
    } /* while */
} /* main */


//
// Helper functions

void compose_top_row(char *strline0) {
    // tempo in BPM
    if (PAR.clock_func == 0)
        sprintf(strline0, "Tr1&2:%3d BPM", ((10000/bpm_set_tempo1)*60)/40);
    else
        sprintf(strline0, "Tr1:%3d Tr2:%3d BPM", ((10000/bpm_set_tempo1)*60)/40, ((10000/bpm_set_tempo2)*60)/40);
}

void compose_sequencer_data(uint track, char *strline1, char*strline2) {
    char str_cv[15];
    char str_cyc[15];
    char notestr[4];
    // upper sequencer row
    if (((PAR.pattern_prop[curr_pattern[track]] & PTRN_CHAIN_MASK) >>5) != curr_pattern[track])
        sprintf(str_cyc, "CYC:%02d/%02d", curr_repeat[track]+1, 
            ((PAR.pattern_prop[curr_pattern[track]] & PTRN_REPT_MASK) >>11)+1);
    else
        strcpy(str_cyc, "Repeating");

    sprintf(strline1, "PTN:%03d%c%s", curr_pattern[track], 
        (PAR.pattern_prop[curr_pattern[track]] & PTRN_RESET_MASK) ? '*' : ' ',
        str_cyc);
        
        // lower sequencer row
    if (PAR.pattern[curr_pattern[track]][curr_step[track]].vcf_cv > 0)
        sprintf(str_cv, "V%3d",PAR.pattern[curr_pattern[track]][curr_step[track]].vcf_cv);
    else
        strcpy(str_cv, "-");

    get_note_string(notestr, PAR.pattern[curr_pattern[track]][curr_step[track]].note & NOTE_MASK);

    sprintf(strline2, "STEP:%02d:%s %c%c%c%c%s", curr_step[track]+1, notestr,
        (PAR.pattern[curr_pattern[track]][curr_step[track]].accent & TIED_MASK) ? 'T' : '-',
        (PAR.pattern[curr_pattern[track]][curr_step[track]].accent & SLIDE_MASK) ? 'S' : '-',
        (PAR.pattern[curr_pattern[track]][curr_step[track]].accent & TRIG_MASK) ? 'P' : '-',
        (PAR.pattern[curr_pattern[track]][curr_step[track]].accent & ACC_MASK) ? 'A' : '-', str_cv);
}

// set the range for the encoder depening on parameter
void select_edit_parameter(uint sel_pattern, uint sel_edit_step, uint sel_param)
{
    switch (sel_param) {
        case e_edit_noteval:
            encoder_set_data(PAR.pattern[sel_pattern][sel_edit_step-1].note & NOTE_MASK, 0, 60);
            break;
        case e_edit_tied:
            encoder_set_data((PAR.pattern[sel_pattern][sel_edit_step-1].accent & TIED_MASK) ? 1 : 0, 0, 1);
            break;
        case e_edit_accent:
            encoder_set_data(PAR.pattern[sel_pattern][sel_edit_step-1].accent & ACC_MASK, 0, 15);
            break;
        case e_edit_slide:
            encoder_set_data((PAR.pattern[sel_pattern][sel_edit_step-1].accent & SLIDE_MASK) ? 1 : 0, 0, 1);
            break;
        case e_edit_trig:
            encoder_set_data((PAR.pattern[sel_pattern][sel_edit_step-1].accent & TRIG_MASK) ? 1 : 0, 0, 1);
            break;
        case e_edit_cv:
            encoder_set_data(PAR.pattern[sel_pattern][sel_edit_step-1].vcf_cv, 0, 100);
        default:
            break;
    }
}

void goto_sys_menu(uint *mmi, uint *sysmenu)
{
    encoder_set_data((SYSMENU_ITEMS*500)+(*sysmenu), 0, SYSMENU_ITEMS*1000);    // give menu spinner a high starting value 
                                                                                // so it will not go negative
    *mmi = m_sys_menu_sel;
    clear_button_event(ALL_EV);
}

void goto_patt_sel_menu(uint *mmi, uint8_t *sel_pattern)
{
    *mmi = m_patt_menu_sel;
    clear_button_event(ALL_EV);
    *sel_pattern = curr_pattern[sel_seqencer];
    encoder_set_data(*sel_pattern, 0, MAXPATTERN-1);
    led_set_status(LED_PATTERN, green);
    led_set_status(LED_EDIT, green);
}

// check if equal for floats
bool fequal(float a, float b, float epsilon) {
    return fabs(a-b) < epsilon;
}

// control gate out pin
void set_gate_output(uint track, uint value) {
    if (track == TRK1)
        gpio_put(TRK1_GATE, value);
    else
        gpio_put(TRK2_GATE, value);
}

// control trig out pin
void set_trig_output(bool value) {
    gpio_put(TRK1_TRIG, value);
}

// convert a note number into 16-bit PWM value
uint16_t note_to_cv(uint track, uint8_t note) {
    float vout = (note > 1) ? ((float)(note-1) * MILLIVOLT_PER_NOTESTEP) : 0.0f; 
    return (uint16_t)constrain((vout * ((float)CAL.mstep_mv[track] / 1000.0f)) + CAL.offs_100mv[track], 0, 0xFFFF);
}

// convert a note number into a string
void get_note_string(char *string, uint8_t note) {
    if (note == 0)
        strcpy(string, "---");
    else
        sprintf(string, "%s%1d", notes[(note-1)%12], (note-1)/12);
}
