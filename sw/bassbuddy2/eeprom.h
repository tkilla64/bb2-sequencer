/** 
* @file eeprom.h
* 
* functions for handling I2C EEPROM (24LC32A)
*
*/

#ifndef _inc_eeprom
#define _inc_eeprom

#include <stdio.h>
#include "sequencer.h"

#define I2C_EEPROM		i2c1
#define I2C_EE_BUSSPD   400000

// 24LC32A specific definitions (se datasheet)
#define EE_SIZE_MIN 2048	// minimum EEPROM size 
#define EE_SIZE_MAX	65536	// maximum EEPROM size
#define EE_ADDR 0b01010000	// Control byte: 1010 + A2-0
#define EE_PAGESIZE	32		// 32 bytes pagesize

// Application specific EE parameters
#define EE_CAL	0x000		// memory offset for CAL data
#define EE_PAR	0x040		// memory offset for PAR data
// Update magic number is you change the contents of the EE parameter storage!
#define CAL_MAGIC_NO    0x1234	// magic number for cal EE parameters
#define PAR_MAGIC_NO    0x2345  // magic number for patterns and user settings EE parameters
// Default startup patterns
#define TRK1_START_PATTERN 0
#define TRK2_START_PATTERN 30

typedef struct nonVolCal 
{
	// CV outputs calibration data
	int16_t	  offs_100mv[2];	// offset around 100 mV
  	uint16_t  mstep_mv[2];		// millisteps per mV

	uint16_t  magic;            // magic number
} nonVolCal_t;

typedef struct nonVolPar 
{
	// sequencer patterns
	seq_step_t pattern[MAXPATTERN][MAXSTEPS];	// pattern data
	uint16_t pattern_prop[MAXPATTERN];			// pattern properties

	// user settings parameters
	bool  clock_pol;		// Clock Input polarity, 0 = low-to-high transition
	bool  reset_pol;		// Reset Input polarity, 0 = low-to-high transition
	uint8_t clock_func;		// Clock Input function, 0 = Clock 1&2 , 1 = Clock 1
	uint8_t reset_func;		// Reset Input function, 0 = Reset 1&2 , 1 = Clock 2
	uint8_t tempo;			// 0=ext clock, 1-255 internal clock (BPM)
	uint8_t seq_ptn_start[2];	// startup pattern for each sequencer 0-59

	uint16_t  magic;		// magic number
} nonVolPar_t;

/**
	@brief init default parameters

	@param[in] nvp  : pointer to cal data array
*/
void eeprom_set_default_cal_data(nonVolCal_t *nvp);

/**
	@brief init default parameters

	@param[in] nvp  : pointer to user parameters data array
*/
void eeprom_set_default_par_data(nonVolPar_t *nvp);

/**
	@brief read parameters from EEPROM

	@param[in] nvp    : pointer to parameter data array
	@param[in] len    : length of array to read in bytes
	@param[in] eeaddr : starting address in EEPROM

 	@return bool
	@retval true if error occured
	@retval false if OK
*/
bool eeprom_read_data(uint8_t *nvp, uint len, uint16_t eeaddr);

/**
	@brief write parameters to EEPROM

	Suitable for writing small amount of data

	@param[in] nvp    : pointer to parameter data array
	@param[in] len    : length of array to write in bytes
	@param[in] eeaddr : starting address in EEPROM

 	@return bool
	@retval true if error occured 
	@retval false if OK
*/
bool eeprom_write_data(uint8_t *nvp, uint len, uint16_t eeaddr);

/**
	@brief write parameters to EEPROM using page write mode

	Intended for bulk write operations
	
	@param[in] nvp    : pointer to parameter data array
	@param[in] len    : length of array to write in bytes
	@param[in] eeaddr : starting address in EEPROM

 	@return bool
	@retval true if error occured 
	@retval false if OK
*/
bool eeprom_page_write_data(uint8_t *nvp, uint len, uint16_t eeaddr);

/**
	@brief probe size of EEPROM

 	@return uint32_t
	@retval EE size in bytes
	@retval 0 if no memory is detected
*/
uint32_t eeprom_size_probe(void);

#endif