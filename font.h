/** \file
 * Bitmap font driver.
 */
#ifndef _model100_font_h_
#define _model100_font_h_

#include <avr/io.h>
#include <stdint.h>

#define FONT_NORMAL	0x00
#define FONT_INVERSE	0x01
#define FONT_UNDERLINE	0x02


extern void
font_draw(
	uint8_t col,
	uint8_t row,
	uint8_t c,
	uint8_t mod
);


#endif
