/** 
* @file sequencer.h
* 
* sequencer logic and helper functions
*
*/

#ifndef _inc_seq
#define _inc_seq

#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"

// Define IO pins
#define TRK2_GATE       8       // Track2 Gate Output
#define TRK1_TRIG       20      // Track1 Trig Output
#define TRK1_GATE       21      // Track1 Gate Output

// definition of patterns
#define MAXSTEPS		16			// max number of steps in a pattern 
#define MAXPATTERN		60			// max number of patterns
#define MAXCYCLES		32			// max number of repeat cycles
#define PRESETS 		5			// preset patterns
#define PTRN_LEN_MASK	0b0000000000001111	// pattern length-1
#define PTRN_RESET_MASK	0b0000000000010000  // reset waypoint in chain
#define PTRN_CHAIN_MASK	0b0000011111100000	// next pattern in chain
#define PTRN_REPT_MASK	0b1111100000000000	// pattern repeat cycles

// definition of steps 
#define NOTE_MASK 		0b00111111	// Note: 0=rest, 1=C-0
#define GATE_START_MASK	0b00001111  // Gate start: 0-15
#define GATE_LEN_MASK	0b11110000  // Gate length: 0-15
#define ACC_MASK		0b00001111	// Accent: value 0-15
#define SLIDE_MASK		0b00010000  // Slide: 0=off, 1=slide to next note
#define TIED_MASK		0b00100000	// Tied: 0=off, 1=tied to next note
#define TRIG_MASK		0b01000000	// Trig: 0=off, 1=on
#define VCF_CV_MASK		0b01111111  // CV value 0-100

// definition of tracks
#define TRK1			0
#define TRK2			1

typedef struct step {
	uint8_t	note;		// NOTE_MASK
	uint8_t gate;		// GATE_START_MASK, GATE_LEN_MASK
	uint8_t accent;		// ACC_MASK, SLIDE_MASK, TIED_MASK, TRIG_MASK
	uint8_t vcf_cv;		// VCF_CV_MASK
} seq_step_t;

#endif