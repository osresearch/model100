/**
 * \file Model 100 motherboard.
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
#include "vt100.h"
#include "lcd.h"
#include "font.h"
#include "keyboard.h"


#define LED		0xD6

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


static void
fill_screen(void)
{
	static uint8_t val;

#if 1
	for (uint8_t j = 0 ; j < 8 ; j++)
	{
		for (uint8_t i = 0 ; i < 40 ; i++)
		{
			val = (val + 1) & 0x3F;
			font_draw(i, j, val + '0', FONT_NORMAL);
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
		vt100_clear();
		vt100_goto(0, 0);
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



/** Hardware serial port interrupt handler */
#define RX_QUEUE_SIZE 128
static volatile uint8_t rx_head; // where the next will be written
static volatile uint8_t rx_tail; // where the next will be read
static uint8_t rx_buf[RX_QUEUE_SIZE];

ISR(USART1_RX_vect)
{ 
	char c = UDR1;
	if (rx_head == rx_tail + RX_QUEUE_SIZE)
		return;
	rx_buf[rx_head++ % RX_QUEUE_SIZE] = c;
}


int
serial_getchar(void)
{
	uint8_t tail = rx_tail;
	if (rx_head == tail)
		return -1;
	char c = rx_buf[tail % RX_QUEUE_SIZE];
	rx_tail = tail + 1;
	return c;
}


int
main(void)
{
	// set for 16 MHz clock
#define CPU_PRESCALE(n) (CLKPR = 0x80, CLKPR = (n))
	CPU_PRESCALE(0);

	// Disable the ADC
	ADMUX = 0;

#ifdef CONFIG_USB_SERIAL
	// initialize the USB, and then wait for the host
	// to set configuration.  If the Teensy is powered
	// without a PC connected to the USB port, this 
	// will wait forever.
	usb_init();
#else
	// We're not using USB, so setup the normal serial port
	// 115.2k, n81, no interrupts
	UBRR1 = 8; // 115.2k
	UCSR1B = (1 << RXEN1) | (1 << TXEN1) | (1 << RXCIE1);
	UCSR1C = (0 << USBS1) | (3 << UCSZ10);
	sei();
#endif

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

#ifdef CONFIG_USB_SERIAL
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
#endif

	fill_screen();

	uint8_t last_key = 0;

	while (1)
	{
#ifdef CONFIG_USB_SERIAL
		int c = usb_serial_getchar();
#else
		int c = serial_getchar();
#endif
		if (c != -1)
		{
			vt100_putc(c);
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
#ifdef CONFIG_USB_SERIAL
				// Normal, send it USB
				usb_serial_putchar(key);
#else
				// Normal, send it serial
				while (bit_is_clear(UCSR1A, UDRE1))
					;
				UDR1 = key;
#endif
			}
		}

		if (bit_is_clear(TIFR0, OCF0A))
			continue;

		sbi(TIFR0, OCF0A); // reset the bit
	}
}
