/**
 * \file MDLS-4022 LCD driver.
 *
 * Preparation for Model 100 retrofit
 *
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

#define LCD_VO		0xB5 // Analog voltage to control contrast
#define LCD_RS		0xF0
#define LCD_RW		0xF1
#define LCD_EN		0xD1
#define LCD_D4		0xF4
#define LCD_D5		0xF5
#define LCD_D6		0xF6
#define LCD_D7		0xF7


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
#define CHAR_RAM 0x18


#define ADDR_0	0xF4
#define ADDR_1	0xF5
#define ADDR_2	0xF6
#define ADDR_3	0xF7
#define ADDR_4	0xD7

#define DATA_0	0xC6
#define DATA_1	0xD3
#define DATA_2	0xD0
#define DATA_3	0xB7
#define DATA_4	0xB3
#define DATA_5	0xB2
#define DATA_6	0xB1
#define DATA_7	0xB0

#define FLASH	0xF1
#define RESET	0xF0
#define WRCE	0xD5
#define RD	0xC7
#define CLKSEL	0xD6

#define BUTTON	0xD1

static const char numbers[][16] = {
	"",
	"one",
	"two",
	"three",
	"four",
	"five",
	"six",
	"seven",
	"eight",
	"nine",
	"ten",
	"eleven",
	"twelve",
	"thirteen",
	"fourteen",
	"a quarter",
	"sixteen",
	"seventeen",
	"eightteen",
	"nineteen",
	"twenty",
	"twenty one",
	"twenty two",
	"twenty three",
	"twenty four",
	"twenty five",
	"twenty six",
	"twenty seven",
	"twenty eight",
	"twenty nine",
	"half",
};


static void
lcd_write(
	uint8_t addr,
	uint8_t data
)
{
	out(ADDR_0, addr & 1); addr >>= 1;
	out(ADDR_1, addr & 1); addr >>= 1;
	out(ADDR_2, addr & 1); addr >>= 1;
	out(ADDR_3, addr & 1); addr >>= 1;
	out(ADDR_4, addr & 1); addr >>= 1;

	out(DATA_0, data & 1); data >>= 1;
	out(DATA_1, data & 1); data >>= 1;
	out(DATA_2, data & 1); data >>= 1;
	out(DATA_3, data & 1); data >>= 1;
	out(DATA_4, data & 1); data >>= 1;
	out(DATA_5, data & 1); data >>= 1;
	out(DATA_6, data & 1); data >>= 1;
	out(DATA_7, data & 1); data >>= 1;

	//_delay_us(100);
	out(WRCE, 0);
	_delay_us(10);
	out(WRCE, 1);
}

static uint8_t ms;
static uint8_t sec;
static uint8_t min;
static uint8_t hour;

static void
update_time(void)
{
	if (++ms < 125)
		return;

	ms = 0;
	if (++sec< 60)
		return;

	sec = 0;
	if (++min < 60)
		return;

	min = 0;
	if (++hour < 24)
		return;

	hour = 0;
}


static char scroll_output[64];
static uint8_t scroll_offset;

static void
scroll(void)
{
	uint8_t did_reset = 0;

	for (int i = 0 ; i < 8 ; i++)
	{
		char c = scroll_output[scroll_offset+i];
		if (did_reset || c == '\0')
		{
			did_reset = 1;
			c = ' ';
		}
	
		lcd_write(CHAR_RAM + i, c);
	}

	if (did_reset)
		scroll_offset = 0;
	else
		scroll_offset++;
}


static void
draw_hms(void)
{
	lcd_write(CHAR_RAM | 0, '0' + (hour / 10) % 10);
	lcd_write(CHAR_RAM | 1, '0' + (hour / 1) % 10);
	lcd_write(CHAR_RAM | 2, ':');
	lcd_write(CHAR_RAM | 3, '0' + (min / 10) % 10);
	lcd_write(CHAR_RAM | 4, '0' + (min / 1) % 10);
	lcd_write(CHAR_RAM | 5, ':');
	lcd_write(CHAR_RAM | 6, '0' + (sec / 10) % 10);
	lcd_write(CHAR_RAM | 7, '0' + (sec / 1) % 10);
}


static void
hour_output(
	uint8_t hour
)
{
	if (hour == 0 || hour == 24)
	{
		strcat(scroll_output, "midnight");
	} else
	if (hour < 12)
	{
		strcat(scroll_output, numbers[hour]);
		strcat(scroll_output, " in the morning");
	} else
	if (hour == 12)
	{
		strcat(scroll_output, "noon");
	} else
	if (hour < 17)
	{
		strcat(scroll_output, numbers[hour-12]);
		strcat(scroll_output, " in the afternoon");
	} else {
		strcat(scroll_output, numbers[hour-12]);
		strcat(scroll_output, " in the evening");
	}
}

static void
word_clock(void)
{
	scroll_output[0] = '\0';
	strcat(scroll_output, "       ");

	if (min <= 30)
	{
		if (min != 0)
		{
			strcat(scroll_output, numbers[min]);
			strcat(scroll_output, " past ");
		}

		hour_output(hour);
	} else {
		strcat(scroll_output, numbers[60-min]);
		strcat(scroll_output, " before ");
		hour_output(hour + 1);
	}

	strcat(scroll_output, "      ");
}
#endif


static void
lcd_command(
	const uint8_t byte,
	const uint8_t rs
)
{
	out(LCD_RS, rs);

	out(LCD_D7, byte & 0x80);
	out(LCD_D6, byte & 0x40);
	out(LCD_D5, byte & 0x20);
	out(LCD_D4, byte & 0x10);
	out(LCD_EN, 1);
	_delay_us(10);
	out(LCD_EN, 0);

	out(LCD_D7, byte & 0x08);
	out(LCD_D6, byte & 0x04);
	out(LCD_D5, byte & 0x02);
	out(LCD_D4, byte & 0x01);
	out(LCD_EN, 1);
	_delay_us(10);
	out(LCD_EN, 0);

	// Rate limit if this is a command, not a text write
	if (rs == 0)
		_delay_ms(5);
	else
		_delay_us(5);
}


static void
lcd_write(
	const uint8_t byte
)
{
	lcd_command(byte, 1);
}

static void
lcd_contrast(
	uint8_t x
)
{
	OCR1A = x;
}

static void
lcd_init(void)
{
	ddr(LCD_D4, 1);
	ddr(LCD_D5, 1);
	ddr(LCD_D6, 1);
	ddr(LCD_D7, 1);
	ddr(LCD_RS, 1);
	ddr(LCD_RW, 1);
	ddr(LCD_EN, 1);
	ddr(LCD_VO, 1);

	out(LCD_D4, 0);
	out(LCD_D5, 0);
	out(LCD_D6, 0);
	out(LCD_D7, 0);
	out(LCD_RS, 0);
	out(LCD_RW, 0);
	out(LCD_EN, 0);

	// PB5, OC1A is used to control brightness via PWM
	// Configure OC1x in fast-PWM mode, 10-bit
	sbi(TCCR1B, WGM12);
	sbi(TCCR1A, WGM11);
	sbi(TCCR1A, WGM10);

	// Configure output mode to clear on match, set at top
	sbi(TCCR1A, COM1A1);
	cbi(TCCR1A, COM1A0);

	// Configure clock 1 at clk/1
	cbi(TCCR1B, CS12);
	cbi(TCCR1B, CS11);
	sbi(TCCR1B, CS10);

	lcd_contrast(20);
	
	_delay_ms(20);

	for (int i = 0 ; i < 3 ; i++)
	{
		// Enable the display
		out(LCD_D7, 0);
		out(LCD_D6, 0);
		out(LCD_D5, 1); // enable
		out(LCD_D4, 1); // 8-bit mode, ignored for now

		out(LCD_EN, 1);
		_delay_us(10);
		out(LCD_EN, 0);
		_delay_ms(5);
	}

	// Enable the display
	out(LCD_D7, 0);
	out(LCD_D6, 0);
	out(LCD_D5, 1); // enable
	out(LCD_D4, 0); // 4-bit mode, for real this time

	out(LCD_EN, 1);
	_delay_us(10);
	out(LCD_EN, 0);
	_delay_ms(5);

	// We should be alive in 4-bit mode now.
	lcd_command(0x28, 0); // 4 bit, 2 line, normal font
	lcd_command(0x01, 0); // clear and home
	lcd_command(0x06, 0); // cursor moves to the right
	lcd_command(0x0C, 0); // turn on display
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

	lcd_write('M');
	lcd_write('o');
	lcd_write('d');
	lcd_write('e');
	lcd_write('l');
	lcd_write('1');
	lcd_write('0');
	lcd_write('0');
	lcd_write('!');

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

	while (1)
	{
		int c = usb_serial_getchar();
		if (c != -1)
		{
			usb_serial_putchar(c);
			if (c == '+')
				OCR1A += 8;
			lcd_write(c);
		}

		if (bit_is_clear(TIFR0, OCF0A))
			continue;

		sbi(TIFR0, OCF0A); // reset the bit
	}
}
