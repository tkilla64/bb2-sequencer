/** 
* @file eeprom.c
* 
* functions for handling I2C EEPROM (24LC32A)
*
* TODO:
*  - Use Ack Polling (chapter 7.0 in DS)
*/

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "eeprom.h"
#include "presets.h"

/*
 * Prime EEPROM with default values
 */

void eeprom_set_default_cal_data(nonVolCal_t *nvp)
{
    nvp->offs_100mv[0] = 0;
    nvp->mstep_mv[0] = 13063;
    nvp->offs_100mv[1] = 0;
    nvp->mstep_mv[1] = 13063;
    nvp->magic = CAL_MAGIC_NO;
}

void eeprom_set_default_par_data(nonVolPar_t *nvp)
{
    nvp->clock_pol = 0;             // Pos edge trigger
    nvp->reset_pol = 0;             // Pos edge trigger
    nvp->clock_func = 0;            // Clock Track 1&2
    nvp->reset_func = 0;            // Reset Track 1&2
    nvp->tempo = 0;                 // Select Ext clock
    nvp->seq_ptn_start[0] = TRK1_START_PATTERN; // Track1 seq startup pattern
    nvp->seq_ptn_start[1] = TRK2_START_PATTERN; // Track2 seq startup pattern

    // apply preset pattern data
    for (int ptn=0 ; ptn < PRESETS ; ptn++) {
        for (int stp=0 ; stp < MAXSTEPS ; stp++) {
            nvp->pattern[ptn][stp].accent = pres_ptn[ptn][stp].accent;
            nvp->pattern[ptn][stp].note = pres_ptn[ptn][stp].note;
            nvp->pattern[ptn][stp].gate = pres_ptn[ptn][stp].gate;
            nvp->pattern[ptn][stp].vcf_cv = pres_ptn[ptn][stp].vcf_cv;
        }
        nvp->pattern_prop[ptn] = pres_prop[ptn];
    }
    // clear rest of the patterns
    for (int ptn=PRESETS ; ptn < MAXPATTERN ; ptn++) {
        for (int stp=0 ; stp < MAXSTEPS ; stp++) {
            nvp->pattern[ptn][stp].accent = 0;
            nvp->pattern[ptn][stp].note = 0;
            nvp->pattern[ptn][stp].gate = 0;
            nvp->pattern[ptn][stp].vcf_cv = 0;
        }
        nvp->pattern_prop[ptn] = (MAXSTEPS-1) | (ptn<<5); // max length, repeat same ptn
        if (ptn == TRK1_START_PATTERN || ptn == TRK2_START_PATTERN)
            nvp->pattern_prop[ptn] = nvp->pattern_prop[ptn] | PTRN_RESET_MASK; // set RESET bit if startup patterns
    }
    nvp->magic = PAR_MAGIC_NO;
}

/*
 * EEPROM utilities
 */

bool eeprom_read_data(uint8_t *nvp, uint len, uint16_t eeaddr)
{
    uint8_t addr[2];
    uint8_t *bp = (uint8_t *) nvp;
    bool ret_val = false;

    // Sequential Read (chapter 8.3 in DS)
    addr[0] = (eeaddr >> 8);
    addr[1] = (eeaddr & 0xFF);
    if (i2c_write_blocking(I2C_EEPROM, EE_ADDR, &addr[0], 2, true) == PICO_ERROR_GENERIC) {
        printf("%s:error setting start addr to EE\n",__func__);
        ret_val = true;
    } 
    else 
    {
        if (i2c_read_blocking(I2C_EEPROM, EE_ADDR, &bp[0], len, false) == PICO_ERROR_GENERIC) {
            printf("%s:error reading data from EE\n",__func__);
            ret_val = true;
        }
    }
    return ret_val;
}

bool eeprom_write_data(uint8_t *nvp, uint len, uint16_t eeaddr)
{
    uint8_t msg_buffer[3];
    uint8_t *bp = (uint8_t *) nvp;
    bool ret_val = false;

    // Byte Write (chapter 6.1 in DS)
    for (int i = 0 ; i < len ; i++) {
        msg_buffer[0] = (i+eeaddr) >> 8;
        msg_buffer[1] = (i+eeaddr) & 0xFF;
        msg_buffer[2] = bp[i];
        if (i2c_write_blocking(I2C_EEPROM, EE_ADDR, &msg_buffer[0], 3, false) == PICO_ERROR_GENERIC) {
            printf("%s:error writing data to EE\n",__func__);
            ret_val = true;
            break;
        }
        sleep_ms(4);
    }
    return ret_val;
}

bool eeprom_page_write_data(uint8_t *nvp, uint len, uint16_t eeaddr)
{
    uint8_t msg_buffer[EE_PAGESIZE+4];
    uint8_t *bp = (uint8_t *) nvp;
    bool ret_val = false;
    uint page = (eeaddr & ~(EE_PAGESIZE-1));
    uint byte = eeaddr % EE_PAGESIZE;
    uint msgbyte;

    // Page Write (chapter 6.2 in DS)
    for (int i = 0 ; i < len ; i++) {
        msgbyte = (i % EE_PAGESIZE)+1;
        page = ((eeaddr+i) & ~(EE_PAGESIZE-1));
        msg_buffer[1+msgbyte] = bp[i];
        if ((eeaddr+i) % EE_PAGESIZE == EE_PAGESIZE-1) {    // reached end of page
            // set page start address
            msg_buffer[0] = (page+byte) >> 8;
            msg_buffer[1] = (page+byte) & 0xFF;
            // send buffer
            if (i2c_write_blocking(I2C_EEPROM, EE_ADDR, &msg_buffer[0], msgbyte+2, false) == PICO_ERROR_GENERIC) {
                printf("%s:error writing data to EE\n",__func__);
                ret_val = true;
                break;
            }            
            sleep_ms(5);
            byte = 0;
        }
    }
    // check if anything left to send
    if (msgbyte > 0) {
        msg_buffer[0] = (page+byte) >> 8;
        msg_buffer[1] = (page+byte) & 0xFF;
        if (i2c_write_blocking(I2C_EEPROM, EE_ADDR, &msg_buffer[0], msgbyte+2, false) == PICO_ERROR_GENERIC) {
            printf("%s:error writing data to EE\n",__func__);
            ret_val = true;
        }            
    }
    return ret_val;
}

uint32_t eeprom_size_probe(void)
{
    struct nonVolCal tmpCAL;    // dummy buffer
    uint32_t ret_val = 0;

    for (uint32_t eeaddr = EE_SIZE_MIN ; eeaddr < EE_SIZE_MAX ; eeaddr *= 2) {
        if (eeprom_read_data((uint8_t *)&tmpCAL, sizeof(tmpCAL), eeaddr))
            break;  // exit if no answer from EEPROM

        // magic number is known data that should only exist at start of memory
        if (tmpCAL.magic == CAL_MAGIC_NO) {
            ret_val = eeaddr;
            break;  // memory is repeating, exit with current size
        }
    }
    return ret_val;
}
