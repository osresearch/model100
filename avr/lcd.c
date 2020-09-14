/**
 * \file HD44102 driver
 *
 * Preparation for Model 100 retrofit
 *
 * CS1 is tied to ground on all chips.
 * CS2 is exposed per chip
 * CS3 is common to all chips, named CS1 on schematic
 * 
 * To select a chip, CS2 and CS3 must be high
 *
 * In write mode, data is latched on the fall of LCD_EN
 * LCD_DI high == data, low == command
 *
 * Keep free:
 * i2c: PD0, PD1
 * RS232: PD2, PD3
 * SPI: PB3, PB2, PB1, PB0
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <stdint.h>
#include <string.h>
#include <util/delay.h>
#include "bits.h"
#include "lcd.h"

#define LCD_V2		0xB7 // 4, Analog voltage to generate negative voltage
#define LCD_VO		0xB6 // Analog voltage to control contrast
#define LCD_RESET	0xD4 // 17
#define LCD_CS1		0xD5 // 18
#define LCD_EN		0xE0 // 19
#define LCD_RW		0xD7 // 20
#define LCD_DI		0xE1 // 21
#define LCD_BZ		0xB5 // 2

#define LCD_DATA_PORT	PORTC // 22-29
#define LCD_DATA_PIN	PINC
#define LCD_DATA_DDR	DDRC

#define LCD_CS20	0xF0
#define LCD_CS21	0xF1
#define LCD_CS22	0xF2
#define LCD_CS23	0xF3
#define LCD_CS24	0xF4
#define LCD_CS25	0xF5
#define LCD_CS26	0xF6
#define LCD_CS27	0xF7
#define LCD_CS28	0xE6
#define LCD_CS29	0xE7


static uint8_t
lcd_command(
	const uint8_t byte,
	const uint8_t di, // 0 == instruction, 1 == data
	const uint8_t write_dir
)
{
	uint8_t rc = 0;

	out(LCD_DI, di);
	out(LCD_RW, !write_dir); // write
	if (write_dir)
	{
		out(LCD_EN, 1);
		LCD_DATA_DDR = 0xFF;
	} else {
		LCD_DATA_DDR = 0x00; // inputs
		LCD_DATA_PORT = 0x00; // no pull ups
		out(LCD_EN, 1);
	}

	if (write_dir)
	{
		_delay_us(2);
		LCD_DATA_PORT = byte;
	} else {
		_delay_us(2);
		rc = LCD_DATA_PIN;
	}

	_delay_us(2);
	out(LCD_EN, 0);

	// value has been sent, go into read mode
	_delay_us(2);
	LCD_DATA_PORT = 0x00; // no pull ups
	LCD_DATA_DDR = 0x00;

	out(LCD_DI, 0); // status command
	out(LCD_RW, 1); // read

	out(LCD_EN, 1);
	_delay_us(10);
	if (write_dir)
		rc = LCD_DATA_PIN;
	out(LCD_EN, 0);

	// Everything looks good.
	out(LCD_RW, 0); // go back into write mode
	return rc;
}



static void
lcd_vee(
	uint16_t x
)
{
	OCR1C = x;
}


static void
lcd_contrast(
	uint16_t x
)
{
	OCR1B = x;
}


static void
lcd_on(
	const uint8_t pin
)
{
	out(pin, 1);

	// Turn on display
	lcd_command(0x39, 0, 1);
	_delay_ms(1);

	// Up mode
	lcd_command(0x3B, 0, 1);
	_delay_ms(1);

	// Start at location 0
	lcd_command(0x00, 0, 1);
	_delay_ms(1);

	// Display start page 0
	lcd_command(0x3E, 0, 1);
	_delay_ms(1);

	out(pin, 0);
}


void
lcd_init(void)
{
	LCD_DATA_PORT = 0x00;
	LCD_DATA_DDR = 0x00;

	out(LCD_DI, 0);
	out(LCD_RW, 0);
	out(LCD_EN, 0);
	out(LCD_V2, 0);
	out(LCD_VO, 0);
	out(LCD_DI, 0);
	out(LCD_CS1, 0);
	out(LCD_CS20, 0);
	out(LCD_CS21, 0);
	out(LCD_CS22, 0);
	out(LCD_CS23, 0);
	out(LCD_CS24, 0);
	out(LCD_CS25, 0);
	out(LCD_CS26, 0);
	out(LCD_CS27, 0);
	out(LCD_CS28, 0);
	out(LCD_CS29, 0);
	out(LCD_RESET, 0);
	out(LCD_BZ, 0);

	ddr(LCD_DI, 1);
	ddr(LCD_RW, 1);
	ddr(LCD_EN, 1);
	ddr(LCD_V2, 1);
	ddr(LCD_VO, 1);
	ddr(LCD_CS1, 1);
	ddr(LCD_RESET, 1);
	ddr(LCD_BZ, 1);

	ddr(LCD_CS20, 1);
	ddr(LCD_CS21, 1);
	ddr(LCD_CS22, 1);
	ddr(LCD_CS23, 1);
	ddr(LCD_CS24, 1);
	ddr(LCD_CS25, 1);
	ddr(LCD_CS26, 1);
	ddr(LCD_CS27, 1);
	ddr(LCD_CS28, 1);
	ddr(LCD_CS29, 1);


	// Configure OC1x in fast-PWM mode, 10-bit
	sbi(TCCR1B, WGM12);
	sbi(TCCR1A, WGM11);
	sbi(TCCR1A, WGM10);

	// OC1C is used to generate the Vee via a charge pump
	// Configure output mode to clear on match, set at top
	sbi(TCCR1A, COM1C1);
	cbi(TCCR1A, COM1C0);

	// OC1B is used to control brightness via PWM
	// Configure output mode to clear on match, set at top
	sbi(TCCR1A, COM1B1);
	cbi(TCCR1A, COM1B0);

	// Configure clock 1 at clk/1
	cbi(TCCR1B, CS12);
	cbi(TCCR1B, CS11);
	sbi(TCCR1B, CS10);

	lcd_vee(0x100); // 50% duty cycle
	lcd_contrast(0x280); // almost +5V
	
	_delay_ms(20);

	// Raise the reset line, to bring the chips online
	out(LCD_RESET, 1);

	// Raise the master select line, since we always want to talk to
	// all chips.  We leave it up since we'll be asserting each one
	// individually in a little while.
	out(LCD_CS1, 1);

	lcd_on(LCD_CS20);
	lcd_on(LCD_CS21);
	lcd_on(LCD_CS22);
	lcd_on(LCD_CS23);
	lcd_on(LCD_CS24);
	lcd_on(LCD_CS25);
	lcd_on(LCD_CS26);
	lcd_on(LCD_CS27);
	lcd_on(LCD_CS28);
	lcd_on(LCD_CS29);

	// Bring LCD select back down since we don't want to
	// talk to the LCD while strobing the keyboard.
	out(LCD_CS1, 0);
}


/** Enable the one chip, select the address and send/recv the byte.
 *
 * x goes from 0 to 50, y goes from 0 to 32, rounded to 8.
 */
static void
lcd_bulk_write(
	const uint8_t pin,
	uint8_t x,
	uint8_t y,
	const uint8_t * buf,
	uint8_t n
)
{
	out(pin, 1);
	lcd_command((y >> 3) << 6 | x, 0, 1);

	for (uint8_t i = 0 ; i < n ; i++)
		lcd_command(buf[i], 1, 1);

	out(pin, 0);
}


/** Display val at position x,y.
 *
 * x is ranged 0 to 240, for each pixel
 * y is ranged 0 to 64, rounded to 8
 */
void
lcd_write(
	uint8_t x,
	uint8_t y,
	const uint8_t * buf,
	uint8_t n
)
{
	out(LCD_CS1, 1);

	if (y < 32)
	{
		// Top half of the display
		if (x < 50)
			lcd_bulk_write(LCD_CS20, x - 0, y - 0, buf, n);
		else
		if (x < 100)
			lcd_bulk_write(LCD_CS21, x - 50, y - 0, buf, n);
		else
		if (x < 150)
			lcd_bulk_write(LCD_CS22, x - 100, y - 0, buf, n);
		else
		if (x < 200)
			lcd_bulk_write(LCD_CS23, x - 150, y - 0, buf, n);
		else
			lcd_bulk_write(LCD_CS24, x - 200, y - 0, buf, n);
	} else {
		// Bottom half of the display
		if (x < 50)
			lcd_bulk_write(LCD_CS25, x - 0, y - 32, buf, n);
		else
		if (x < 100)
			lcd_bulk_write(LCD_CS26, x - 50, y - 32, buf, n);
		else
		if (x < 150)
			lcd_bulk_write(LCD_CS27, x - 100, y - 32, buf, n);
		else
		if (x < 200)
			lcd_bulk_write(LCD_CS28, x - 150, y - 32, buf, n);
		else
			lcd_bulk_write(LCD_CS29, x - 200, y - 32, buf, n);
	}

	out(LCD_CS1, 0);
}


/** Enable the one chip, select the address and send/recv the byte.
 *
 * x goes from 0 to 50, y goes from 0 to 32, rounded to 8.
 */
static void
lcd_bulk_read(
	const uint8_t pin,
	uint8_t x,
	uint8_t y,
	uint8_t * buf,
	uint8_t n
)
{
	out(pin, 1);
	lcd_command((y >> 3) << 6 | x, 0, 1);
	lcd_command(0, 1, 0); // dummy

	for (int i = 0 ; i < n ; i++)
		buf[i] = lcd_command(0, 1, 0);
	out(pin, 0);
}


void
lcd_read(
	uint8_t x,
	uint8_t y,
	uint8_t * buf,
	uint8_t n
)
{
	out(LCD_CS1, 1);

	if (y < 32)
	{
		// Top half of the display
		if (x < 50)
			lcd_bulk_read(LCD_CS20, x - 0, y - 0, buf, n);
		else
		if (x < 100)
			lcd_bulk_read(LCD_CS21, x - 50, y - 0, buf, n);
		else
		if (x < 150)
			lcd_bulk_read(LCD_CS22, x - 100, y - 0, buf, n);
		else
		if (x < 200)
			lcd_bulk_read(LCD_CS23, x - 150, y - 0, buf, n);
		else
			lcd_bulk_read(LCD_CS24, x - 200, y - 0, buf, n);
	} else {
		// Bottom half of the display
		if (x < 50)
			lcd_bulk_read(LCD_CS25, x - 0, y - 32, buf, n);
		else
		if (x < 100)
			lcd_bulk_read(LCD_CS26, x - 50, y - 32, buf, n);
		else
		if (x < 150)
			lcd_bulk_read(LCD_CS27, x - 100, y - 32, buf, n);
		else
		if (x < 200)
			lcd_bulk_read(LCD_CS28, x - 150, y - 32, buf, n);
		else
			lcd_bulk_read(LCD_CS29, x - 200, y - 32, buf, n);
	}

	out(LCD_CS1, 0);
}
