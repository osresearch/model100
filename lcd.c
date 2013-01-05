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

#define LCD_CS20	0xF0 // 16
#define LCD_CS21	0xF1 // 16
#define LCD_CS22	0xF2 // 16
#define LCD_CS23	0xF3 // 16
#define LCD_CS24	0xF4 // 16
#define LCD_CS25	0xF5 // 16
#define LCD_CS26	0xF6 // 16
#define LCD_CS27	0xF7 // 16
#define LCD_CS28	0xE6 // 16
#define LCD_CS29	0xE7 // 16

// Shared with LCD data and chip select lines
#define KEY_ROWS_PIN	PINC
#define KEY_ROWS_DDR	DDRC
#define KEY_ROWS_PORT	PORTC

#define KEY_COLS_PIN	PINF
#define KEY_COLS_DDR	DDRF
#define KEY_COLS_PORT	PORTF

void send_str(const char *s);
uint8_t recv_str(char *buf, uint8_t size);
void parse_and_execute_command(const char *buf, uint8_t num);

static uint8_t
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


static uint8_t
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
}


static const uint8_t keycodes[] PROGMEM =
{
	[0x0f] = 'a',
	[0x03] = 'b',
	[0x05] = 'c',
	[0x0d] = 'd',
	[0x15] = 'e',
	[0x0c] = 'f',
	[0x0b] = 'g',
	[0x0a] = 'h',
	[0x10] = 'i',
	[0x09] = 'j',
	[0x08] = 'k',
	[0x00] = 'l',
	[0x01] = 'm',
	[0x02] = 'n',
	[0x1f] = 'o',
	[0x1e] = 'p',
	[0x17] = 'q',
	[0x14] = 'r',
	[0x0e] = 's',
	[0x13] = 't',
	[0x11] = 'u',
	[0x04] = 'v',
	[0x16] = 'w',
	[0x06] = 'x',
	[0x12] = 'y',
	[0x07] = 'z',

	[0x27] = '1',
	[0x26] = '2',
	[0x25] = '3',
	[0x24] = '4',
	[0x23] = '5',
	[0x22] = '6',
	[0x21] = '7',
	[0x20] = '8',
	[0x2f] = '9',
	[0x2e] = '0',
	[0x2d] = '-',
	[0x2c] = '=',

	[0x34] = 27, // escape
	[0x36] = 8, // backspace
	[0x37] = ' ',
	[0x35] = '\t',
	[0x30] = '\n',
	[0x1c] = ';',
	[0x1b] = '\'',
	[0x1a] = ',',
	[0x19] = '.',
	[0x18] = '/',
	[0x1d] = '[',
};

static void
keyboard_reset(void)
{
	KEY_ROWS_DDR = 0x00; // all inputs
	KEY_ROWS_PORT = 0x00; // all tri-state

	KEY_COLS_DDR = 0xFF; // leave as all output
	KEY_COLS_PORT = 0x00; // all low
}


static uint8_t
keyboard_scan(void)
{
	keyboard_init();

	uint8_t mask = 1;
	for (uint8_t col = 0 ; col < 8 ; col++, mask <<= 1)
	{
		KEY_COLS_PORT = ~mask; // pull one down
		_delay_us(1);
		uint8_t rows = ~KEY_ROWS_PIN;

		if (!rows)
			continue;

		keyboard_reset();
		mask = 1;

		for (uint8_t row = 0 ; row < 8 ; row++, mask <<= 1)
		{
			if ((rows & mask) == 0)
				continue;

			return col << 3 | row;
		}
	}

	keyboard_reset();
	return 0xFF;
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

	char buf[16];
	int off = 0;

	while (1)
	{
		int c = usb_serial_getchar();
		if (c != -1)
		{
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

			if (c == ' ')
				redraw();
		}

#if 1
		uint8_t key = keyboard_scan();
		if (key == 0xFF)
		{
			last_key = 0xFF;
		} else
		if (key != last_key)
		{
			last_key = key;
			char c = pgm_read_byte(&keycodes[key]);
			if (c != 0)
			{
				usb_serial_putchar(c);
			} else {
				char buf[16];
				int off = 0;
				buf[off++] = hexdigit(key >> 4);
				buf[off++] = hexdigit(key >> 0);
				buf[off++] = '\r';
				buf[off++] = '\n';
				usb_serial_write(buf, off);
			}
		}
#endif

		if (bit_is_clear(TIFR0, OCF0A))
			continue;

		sbi(TIFR0, OCF0A); // reset the bit
	}
}
