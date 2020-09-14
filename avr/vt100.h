/** \file
 * VT100 terminal emulation.
 */
#ifndef _vt100_
#define _vt100_

#include <avr/io.h>
#include <stdint.h>


extern void
vt100_clear(void);


extern void
vt100_goto(
	uint8_t new_row,
	uint8_t new_col
);


extern void
vt100_putc(
	char c
);


#endif
