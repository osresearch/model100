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


/** Display an array of N columns starting at position x,y */
extern void
lcd_write(
	uint8_t x,
	uint8_t y,
	const uint8_t * val,
	uint8_t n
);


/** Read an array of N colums starting at position x,y */
extern void
lcd_read(
	uint8_t x,
	uint8_t y,
	uint8_t * buf,
	uint8_t n
);


/** Display a single vertical column val at position x,y.
 *
 * x is ranged 0 to 240, for each pixel
 * y is ranged 0 to 64, rounded to 8
 * MSB of val is at the top, LSB is at the bottom.
 */
static inline void
lcd_display(
	uint8_t x,
	uint8_t y,
	uint8_t val
)
{
	lcd_write(x, y, &val, 1);
}

#endif
