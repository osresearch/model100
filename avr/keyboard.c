/**
 * \file Model 100 keyboard matrix reader
 *
 * Keyboard needs:
 *	9 columns, shared with the 10 LCD chipselect lines
 *	8 rows, must not be shared.
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <stdint.h>
#include <string.h>
#include <util/delay.h>
#include "usb_serial.h"
#include "bits.h"
#include "keyboard.h"

// Can not be shared with the LCD
#define KEY_ROWS_PIN	PINA
#define KEY_ROWS_DDR	DDRA
#define KEY_ROWS_PORT	PORTA

// Shared with LCD chip select lines
#define KEY_COLS_PIN	PINF
#define KEY_COLS_DDR	DDRF
#define KEY_COLS_PORT	PORTF
#define KEY_COLS_MOD	0xE6 // shared with LCD_CS8

// The bits on the modifier column
#define KEY_MOD_SHIFT	0x01
#define KEY_MOD_CONTROL	0x02
#define KEY_MOD_GRAPH	0x04
#define KEY_MOD_CODE 	0x08
#define KEY_MOD_NUMLOCK	0x10
#define KEY_MOD_CAPS	0x20
#define KEY_MOD_NC	0x40
#define KEY_MOD_BREAK	0x80


/** Layout of the rows and columns in normal mode */
static const uint8_t key_codes[8][8] PROGMEM =
{
	[0] = "\x81\x82\x83\x84\x85\x86\x87\x88", // function keys
	[1] = "zxcvbnml",
	[2] = "asdfghjk",
	[3] = "qwertyui",
	[4] = "op[;',./",
	[5] = "12345678",
	[6] = "90-=\x92\x93\x90\x91", // need to handle arrows
	[7] = " \x8\t\eLC0\n", // need to handle weird keys
};


/** Layout of the rows and columns when shifted */
static const uint8_t shift_codes[8][8] PROGMEM =
{
	[0] = "\x81\x82\x83\x84\x85\x86\x87\x88", // function keys
	[1] = "ZXCVBNML",
	[2] = "ASDFGHJK",
	[3] = "QWERTYUI",
	[4] = "OP]:\"<>?",
	[5] = "!@#$%^&*",
	[6] = "()_+\x92\x93\x90\x91", // need to handle arrows
	[7] = " \x8\t\eLC0\n", // need to handle weird keys
};


/** Initialize the keyboard for a scan.
 *
 * This is called before each read of the keyboard, unlike the
 * LCD that is initialized at boot time.
 */
static void
keyboard_init(void)
{
	// KEY_Cx configuration is handled in lcd_init() sincej
	// they are shared with the chip select lines of the LCD 
	KEY_ROWS_DDR = 0x00; // all input
	KEY_ROWS_PORT = 0xFF; // all pull ups enabled

	KEY_COLS_DDR = 0xFF; // all output
	KEY_COLS_PORT = 0xFF; // all high

	// Pull the function key line high, too
	ddr(KEY_COLS_MOD, 1);
	out(KEY_COLS_MOD, 1);
}


/** Return the keyboard to the normal state (LCD writing).
 *
 */
static void
keyboard_reset(void)
{
	KEY_ROWS_DDR = 0x00; // all inputs
	KEY_ROWS_PORT = 0x00; // all tri-state

	KEY_COLS_DDR = 0xFF; // leave as all output
	KEY_COLS_PORT = 0x00; // all low

	// Reset the function key modifier to pull down, too
	ddr(KEY_COLS_MOD, 1);
	out(KEY_COLS_MOD, 0);
}



static uint8_t
keyboard_scancode_convert(
	uint8_t col,
	uint8_t rows,
	uint8_t mods
)
{
	for (uint8_t row = 0, mask = 1 ; row < 8 ; row++, mask <<= 1)
	{
		if ((rows & mask) == 0)
			continue;

		char c = pgm_read_byte(&(
			(mods & KEY_MOD_SHIFT) && !(mods & KEY_MOD_CONTROL)
			? shift_codes
			: key_codes
		)[col][row]);

		// If control is held, only allow a-z
		// Send nothing, otherwise
		if (mods & KEY_MOD_CONTROL)
		{
			if ('a' <= c && c <= 'z')
				return 0x1 + c - 'a';
			return 0;
		}

		// If we are caps locked, switch lower and upper
		if (mods & KEY_MOD_CAPS)
		{
			if ('a' <= c && c <= 'z')
				c -= 32;
			else
			if ('A' <= c && c <= 'Z')
				c += 32;
		}

		return c;
	}

	// Scan code converts to nothing...
	return 0;
}


/** Check to see if there are any keys held down.
 *
 * \todo Scans only one column per call?
 *
 * \todo Multiple keys held?
 *
 * \return 0 if no keys are held down
 */
uint8_t
keyboard_scan(void)
{
	keyboard_init();


	// Scan the modifier column first, which is on the separate pin
	out(KEY_COLS_MOD, 0);
	_delay_us(50);
	const uint8_t mods = ~KEY_ROWS_PIN;
	out(KEY_COLS_MOD, 1);

	uint8_t mask = 1;
	for (uint8_t col = 0 ; col < 8 ; col++, mask <<= 1)
	{
		KEY_COLS_PORT = ~mask; // pull one down
		_delay_us(50); // wait for things to stabilize
		uint8_t rows = ~KEY_ROWS_PIN;
		KEY_COLS_PORT = 0xFF; // bring them all back up

		if (!rows)
			continue;

		keyboard_reset();

		return keyboard_scancode_convert(col, rows, mods);
	}

	keyboard_reset();
	return 0;
}
