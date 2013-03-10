/** \file
 * HD44102 LCD driver.
 */
#ifndef _model100_lcd_h_
#define _model100_lcd_h_

#include <avr/io.h>
#include <stdint.h>


/** Bring up the LCD interface.
 *
 * Call once after boot to bring up the interface.
 */
extern void
lcd_init(void);


/** Display vertical column val at position x,y.
 *
 * x is ranged 0 to 240, for each pixel
 * y is ranged 0 to 64, rounded to 8
 * MSB of val is at the top, LSB is at the bottom.
 */
extern void
lcd_display(
	uint8_t x,
	uint8_t y,
	uint8_t val
);


/** Read a vertical colum at position x,y */
extern uint8_t
lcd_read(
	uint8_t x,
	uint8_t y
);


#endif
