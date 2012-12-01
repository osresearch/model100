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

#define LCD_V2		0xB6 // 4, Analog voltage to control contrast
#define LCD_CS20	0xB5 // 16
#define LCD_RESET	0xB4 // 17
#define LCD_CS1		0xB3 // 18
#define LCD_EN		0xB2 // 19
#define LCD_RW		0xB1 // 20
#define LCD_DI		0xB7 // 21
#define LCD_BZ		0xB0 // 2

#define LCD_DATA_PORT	PORTD // 22-29
#define LCD_DATA_PIN	PIND
#define LCD_DATA_DDR	DDRD


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


#if 0
static uint8_t
lcd_read(void)
{
	out(LCD_DI, rs);
	ddr(LCD_DATA_PORT, 0); // all in
	out(LCD_RW, 1); // read

	_delay_us(10);
	out(LCD_EN, 1);
	_delay_us(100);
	out(LCD_EN, 0);
	

	out(LCD_EN, 1);
	LCD_DATA_PORT = byte;
	_delay_us(10);
	out(LCD_EN, 0);
#endif


static uint8_t
lcd_command(
	const uint8_t byte,
	const uint8_t di // 0 == instruction, 1 == data
)
{
	out(LCD_DI, di);
	out(LCD_RW, 0); // write
	LCD_DATA_DDR = 0xFF;

	out(LCD_EN, 1);
	LCD_DATA_PORT = byte;
	_delay_us(100);
	out(LCD_EN, 0);

	// value has been sent, wait a tick, read the status
	_delay_us(50);
	LCD_DATA_PORT = 0x00; // no pull ups
	LCD_DATA_DDR = 0x00;
	_delay_us(25);
	out(LCD_RW, 1); // read
	out(LCD_DI, 0); // status command

	_delay_us(25);
	
	out(LCD_EN, 1);
	_delay_us(1);
	uint8_t rc = LCD_DATA_PIN;
	_delay_us(100);
	out(LCD_EN, 0);

	out(LCD_RW, 0); // read

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
lcd_contrast(
	uint8_t x
)
{
	OCR1B = x;
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
	out(LCD_DI, 0);
	out(LCD_CS1, 0);
	out(LCD_CS20, 0);
	out(LCD_RESET, 0);
	out(LCD_BZ, 0);

	ddr(LCD_DI, 1);
	ddr(LCD_RW, 1);
	ddr(LCD_EN, 1);
	ddr(LCD_V2, 1);
	ddr(LCD_CS1, 1);
	ddr(LCD_CS20, 1);
	ddr(LCD_RESET, 1);
	ddr(LCD_BZ, 1);


	// OC1B is used to control brightness via PWM
	// Configure OC1x in fast-PWM mode, 10-bit
	sbi(TCCR1B, WGM12);
	sbi(TCCR1A, WGM11);
	sbi(TCCR1A, WGM10);

	// Configure output mode to clear on match, set at top
	sbi(TCCR1A, COM1B1);
	cbi(TCCR1A, COM1B0);

	// Configure clock 1 at clk/1
	cbi(TCCR1B, CS12);
	cbi(TCCR1B, CS11);
	sbi(TCCR1B, CS10);

	lcd_contrast(64);
	
	_delay_ms(20);

	// Raise the reset line, to bring the chips online
	out(LCD_RESET, 1);

	// Raise the master select line, since we always want to talk to
	// all chips.
	out(LCD_CS1, 1);
	out(LCD_CS20, 1);

	// Turn on display
	lcd_command(0x39, 0);

	// Up mode
	lcd_command(0x3B, 0);

	// Start at location 0
	lcd_command(0x00, 0);
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
	uint16_t i = 0;

	while (1)
	{
		int c = usb_serial_getchar();
		if (c != -1)
		{
			usb_serial_putchar(c);
			if (c == '+')
			{
				OCR1B += 8;
				out(LCD_BZ, 1);
				_delay_ms(50);
				out(LCD_BZ, 0);
			}
		}

		if (bit_is_clear(TIFR0, OCF0A))
			continue;

		sbi(TIFR0, OCF0A); // reset the bit

		i++;
		if ((i & 0xFF) == 0x00)
			lcd_command((i & 0xFF) >> 2, 0);

		uint8_t r = lcd_write(i++);

		char buf[16];
		int off = 0;
		buf[off++] = hexdigit(r >> 4);
		buf[off++] = hexdigit(r >> 0);
		buf[off++] = ' ';
		buf[off++] = r & 0x80 ? 'B' : ' ';
		buf[off++] = r & 0x40 ? 'U' : ' ';
		buf[off++] = r & 0x20 ? 'O' : ' ';
		buf[off++] = r & 0x10 ? 'R' : ' ';
		buf[off++] = r & 0x08 ? '?' : ' ';
		buf[off++] = r & 0x04 ? '?' : ' ';
		buf[off++] = r & 0x02 ? '?' : ' ';
		buf[off++] = r & 0x01 ? '?' : ' ';
		buf[off++] = '\r';
		buf[off++] = '\n';
		usb_serial_write(buf, off);
	}
}
