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
 *
 * Keyboard needs:
 *	9 column, 8 row: 17
 * LCD needs:
 *	10 select, (could be shared with keyboard?)
 *	8 data
 * 	CS1 
 *	EN, DI, RW: 3
 *	V2: 
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


#define LED		0xD6

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

void send_str(const char *s);
uint8_t recv_str(char *buf, uint8_t size);
void parse_and_execute_command(const char *buf, uint8_t num);

static inline uint8_t
hexdigit(
	uint8_t x
)
{
	x &= 0xF;
	if (x < 0xA)
		return x + '0' - 0x0;
	else
		return x + 'A' - 0xA;
}



// Send a string to the USB serial port.  The string must be in
// flash memory, using PSTR
//
void send_str(const char *s)
{
	char c;
	while (1) {
		c = pgm_read_byte(s++);
		if (!c) break;
		usb_serial_putchar(c);
	}
}


static uint8_t
lcd_command(
	const uint8_t byte,
	const uint8_t di // 0 == instruction, 1 == data
)
{
	out(LCD_DI, di);
	out(LCD_RW, 0); // write
	out(LCD_EN, 1);
	LCD_DATA_DDR = 0xFF;
	_delay_us(2);
	LCD_DATA_PORT = byte;
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
	uint8_t rc = LCD_DATA_PIN;
	out(LCD_EN, 0);

	// Everything looks good.
	out(LCD_RW, 0); // go back into write mode
	return rc;
}


static inline uint8_t
lcd_write(
	const uint8_t byte
)
{
	return lcd_command(byte, 1);
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
	lcd_command(0x39, 0);
	_delay_ms(1);

	// Up mode
	lcd_command(0x3B, 0);
	_delay_ms(1);

	// Start at location 0
	lcd_command(0x00, 0);
	_delay_ms(1);

	// Display start page 0
	lcd_command(0x3E, 0);
	_delay_ms(1);

	out(pin, 0);
}


static void
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


/** Enable the one chip, select the address and send the byte.
 *
 * x goes from 0 to 50, y goes from 0 to 32, rounded to 8.*/
static void
lcd_doit(
	const uint8_t pin,
	uint8_t x,
	uint8_t y,
	uint8_t val
)
{
	out(pin, 1);
	lcd_command((y >> 3) << 6 | x, 0);
	lcd_command(val, 1);
	out(pin, 0);
}


/** Display val at position x,y.
 *
 * x is ranged 0 to 240, for each pixel
 * y is ranged 0 to 64, rounded to 8
 */
static void
lcd_display(
	uint8_t x,
	uint8_t y,
	uint8_t val
)
{
	out(LCD_CS1, 1);

	if (y < 32)
	{
		// Top half of the display
		if (x < 50)
			lcd_doit(LCD_CS20, x - 0, y - 0, val);
		else
		if (x < 100)
			lcd_doit(LCD_CS21, x - 50, y - 0, val);
		else
		if (x < 150)
			lcd_doit(LCD_CS22, x - 100, y - 0, val);
		else
		if (x < 200)
			lcd_doit(LCD_CS23, x - 150, y - 0, val);
		else
			lcd_doit(LCD_CS24, x - 200, y - 0, val);
	} else {
		// Bottom half of the display
		if (x < 50)
			lcd_doit(LCD_CS25, x - 0, y - 32, val);
		else
		if (x < 100)
			lcd_doit(LCD_CS26, x - 50, y - 32, val);
		else
		if (x < 150)
			lcd_doit(LCD_CS27, x - 100, y - 32, val);
		else
		if (x < 200)
			lcd_doit(LCD_CS28, x - 150, y - 32, val);
		else
			lcd_doit(LCD_CS29, x - 200, y - 32, val);
	}

	out(LCD_CS1, 0);
}

#include "font.c"


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


/** Check to see if there are any keys held down.
 *
 * \todo Scans only one column per call?
 *
 * \todo Multiple keys held?
 *
 * \return 0 if no keys are held down
 */
static uint8_t
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
		_delay_us(50);
		uint8_t rows = ~KEY_ROWS_PIN;
		KEY_COLS_PORT = 0xFF;; // bring them all back up

		if (!rows)
			continue;

		keyboard_reset();
		mask = 1;

		for (uint8_t row = 0 ; row < 8 ; row++, mask <<= 1)
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
	}

	keyboard_reset();
	return 0;
}


// These might change if we use a smaller font.
#define MAX_COLS 40
#define MAX_ROWS 8

static uint8_t cur_col;
static uint8_t cur_row;
static uint8_t vt100_state;
static uint8_t vt100_inverse;

static void
lcd_clear(void)
{
	for (uint8_t j = 0 ; j < 8 ; j++)
		for (uint8_t i = 0 ; i < 40 ; i++)
			lcd_char(i, j, ' ');
}


static void
buzzer(void)
{
	// todo: use PWM on the buzzer
}


/** VT100ish emulation.
 *
 * A simplistic approach to VT100 emulation.  Only a few commands
 * are supported:
 *   Erase
 *   Home
 *   Cursor up
 *   Cursor down
 */

static void
vt100_process(
	char c
)
{
	static uint8_t arg1;
	static uint8_t arg2;
	static uint8_t vt100_query;

	if (vt100_state == 1)
	{
		arg1 = arg2 = 0;
		vt100_query = 0;

		if (c == 'c')
		{
			lcd_clear();
			cur_row = cur_col = 0;
		} else
		if (c == '[')
		{
			vt100_state = 2;
			return;
		} else
		if (c == '(' || c == ')')
		{
			// We mostly ignore these
			vt100_state = 10;
			return;
		}
	} else
	if (vt100_state == 2)
	{
		if (c == ';')
		{
			vt100_state = 3;
			return;
		} else
		if (c == '?')
		{
			vt100_query = 1;
			return;
		} else
		if ('0' <= c && c <= '9')
		{
			arg1 = (arg1 * 10) + c - '0';
			return;
		} else
		if (c == 'H')
		{
			// <ESC>[H == home cursor
			cur_row = cur_col = 0;
		} else
		if (c == 'm')
		{
			// <ESC>[{arg}m == set attributes
			// 0 == clear
			if (arg1 == 0)
				vt100_inverse = 0;
		} else
		if (c == 'A')
		{
			// <ESC>[{arg}A == move N lines up
			if (cur_row < arg1)
				cur_row = 0;
			else
				cur_row -= arg1;
		} else
		if (c == 'B')
		{
			// <ESC>[{arg}B == move N lines down
			if (cur_row + arg1 >= MAX_ROWS)
				cur_row = MAX_ROWS-1;
			else
				cur_row += arg1;
		} else
		if (c == 'D')
		{
			// <ESC>[{arg}D == move N lines to the left
			if (cur_col < arg1)
				cur_col = 0;
			else
				cur_col -= arg1;
		} else
		if (c == 'C')
		{
			// <ESC>[{arg}C == move N lines to the right
			if (cur_col + arg1 >= MAX_COLS)
				cur_col = MAX_COLS-1;
			else
				cur_col += arg1;
		} else
		if (c == 'J')
		{
			// <ESC>[{arg}J == clear the screen
			// 0 == to the bottom
			// 1 == to the top
			// 2 == entire screen, and move home
			// we just do a full clear
			lcd_clear();
			cur_row = cur_col = 0;
		} else
		if (c == 'K')
		{
			// <ESC>[K == erase to end of line
			for (uint8_t x = cur_col ; x < MAX_COLS ; x++)
				lcd_char(x, cur_row, ' ');
		}
	} else
	if (vt100_state == 3)
	{
		if ('0' <= c && c <= '9')
		{
			arg2 = (arg2 * 10) + c - '0';
			return;
		} else
		if (c == 'H')
		{
			// <ESC>[{row};{col}H == goto position row,col
			// vt100 is 1 indexed, we are 0 indexed.
			cur_row = arg1 > 0 ? arg1 - 1 : 0;
			cur_col = arg2 > 0 ? arg2 - 1 : 0;
			if (cur_row >= MAX_ROWS)
				cur_row = MAX_ROWS - 1;
			if (cur_col >= MAX_COLS)
				cur_col = MAX_COLS - 1;
		} else
		if (c == 'm')
		{
			// <ESC>[{arg};{arg}m == set attributes
			// 0 == clear
			// no others are supported
			if (arg2 == 7)
				vt100_inverse = 0x80;
			else // if (arg1 == 0)
				vt100_inverse = 0;
		}
	} else
	if (vt100_state == 10)
	{
		// We really don't care.
	}

	// If we have fallen through to here, we are done and should
	// exit vt100 mode.
	vt100_state = 0;
	return;
}


static void
lcd_putc(
	char c
)
{
	if (c == '\e')
	{
		vt100_state = 1;
		return;
	} else
	if (vt100_state)
	{
		vt100_process(c);
		return;
	}

	if (c == '\r')
	{
		cur_col = 0;
	} else
	if (c == '\n')
	{
		goto new_row;
	} else
	if (c == '\x7')
	{
		// Bell!
		buzzer();
	} else
	if (c == '\xF')
	{
		// ^O or ASCII SHIFT-IN (SI) switches sets?
/*
		if (vt100_inverse)
			vt100_inverse = 0;
		else
			vt100_inverse = 0x80;
*/
	} else
 	if (c == '\xE')
	{
		// We do not support alternate char sets for now.
		// ignore.
		// ^P or ASCII SHIFT-OUT (SO) switches sets, too
	} else
	if (c == '\x8')
	{
		// erase the old char and backup
		lcd_char(cur_col, cur_row, ' ');
		if (cur_col > 0)
		{
			cur_col--;
		} else {
			cur_col = 40;
			cur_row = (cur_row - 1 + MAX_ROWS) % MAX_ROWS;
		}
	} else {
		lcd_char(cur_col, cur_row, c | vt100_inverse);
		if (++cur_col == 40)
			goto new_row;
	}

	return;

new_row:
	cur_row = (cur_row + 1) % 8;
	cur_col = 0;
}


static void
redraw(void)
{
	static uint8_t val;

#if 1
	for (uint8_t j = 0 ; j < 8 ; j++)
	{
		for (uint8_t i = 0 ; i < 40 ; i++)
		{
			val = (val + 1) & 0x3F;
			lcd_char(i, j, val + '0');
		}
	}
#else
	for (uint8_t y = 0 ; y < 64 ; y += 8)
	{
		for (uint8_t x = 0 ; x < 240 ; x++)
		{
			lcd_display(x, y, val++);
		}
	}
#endif

	val++;
}


static void
key_special(
	const uint8_t key
)
{
	if (key == 0x81)
	{
		// f1 == redraw everything
		lcd_clear();
		cur_row = cur_col = vt100_state = 0;
		return;
	}

	if (0x90 <= key && key <= 0x93)
	{
		uint8_t buf[2];
		buf[0] = '\e';
		buf[1] = 'A' + key - 0x90;
		usb_serial_write(buf, 2);
		return;
	}
}


int
main(void)
{
	// set for 16 MHz clock
#define CPU_PRESCALE(n) (CLKPR = 0x80, CLKPR = (n))
	CPU_PRESCALE(0);

	// Disable the ADC
	ADMUX = 0;

	// initialize the USB, and then wait for the host
	// to set configuration.  If the Teensy is powered
	// without a PC connected to the USB port, this 
	// will wait forever.
	usb_init();

	// LED is an output; will be pulled down once connected
	ddr(LED, 1);
	out(LED, 1);

	lcd_init();
	//keyboard_init();

        // Timer 0 is used for a 64 Hz control loop timer.
        // Clk/256 == 62.5 KHz, count up to 125 == 500 Hz
        // Clk/1024 == 15.625 KHz, count up to 125 == 125 Hz
        // CTC mode resets the counter when it hits the top
        TCCR0A = 0
                | 1 << WGM01 // select CTC
                | 0 << WGM00
                ;

        TCCR0B = 0
                | 0 << WGM02
                | 1 << CS02 // select Clk/256
                | 0 << CS01
                | 1 << CS00
                ;

        OCR0A = 125;
        sbi(TIFR0, OCF0A); // reset the overflow bit

	while (!usb_configured())
		;

	_delay_ms(1000);

	// wait for the user to run their terminal emulator program
	// which sets DTR to indicate it is ready to receive.
	while (!(usb_serial_get_control() & USB_SERIAL_DTR))
		;

	// discard anything that was received prior.  Sometimes the
	// operating system or other software will send a modem
	// "AT command", which can still be buffered.
	usb_serial_flush_input();

	send_str(PSTR("lcd model100\r\n"));
	redraw();

	uint8_t last_key = 0;

	while (1)
	{
		int c = usb_serial_getchar();
		if (c != -1)
		{
			lcd_putc(c);
/*
			usb_serial_putchar(c);
			if (c == '+')
			{
				OCR1C += 8;
				off = 0;
				buf[off++] = hexdigit(OCR1C >> 8);
				buf[off++] = hexdigit(OCR1C >> 4);
				buf[off++] = hexdigit(OCR1C >> 0);
				buf[off++] = '\r';
				buf[off++] = '\n';
				usb_serial_write(buf, off);
			}
			if (c == '-')
			{
				OCR1B += 8;
				off = 0;
				buf[off++] = hexdigit(OCR1B >> 8);
				buf[off++] = hexdigit(OCR1B >> 4);
				buf[off++] = hexdigit(OCR1B >> 0);
				buf[off++] = '\r';
				buf[off++] = '\n';
				usb_serial_write(buf, off);
			}
*/
		}

		uint8_t key = keyboard_scan();
		if (key == 0)
		{
			last_key = 0;
		} else
		if (key != last_key)
		{
			last_key = key;
			if (key >= 0x80)
			{
				// Special char!
				key_special(key);
			} else {
				// Normal, send it.
				usb_serial_putchar(key);
			}
		}

		if (bit_is_clear(TIFR0, OCF0A))
			continue;

		sbi(TIFR0, OCF0A); // reset the bit
	}
}
