/**
 * \file VT100-like emulation.
 *
 * This implements a very simplistic VT100 emulator.
 * Not very many of the functions are implemented; just enough to
 * run vi and lynx, etc.
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <stdint.h>
#include <string.h>
#include <util/delay.h>
#include "vt100.h"
#include "lcd.h"
#include "font.h"


// These might change if we use a smaller font.
#define MAX_COLS 40
#define MAX_ROWS 8

static uint8_t cur_col;
static uint8_t cur_row;
static uint8_t vt100_state;
static uint8_t font_mod;

void
vt100_clear(void)
{
	for (uint8_t j = 0 ; j < 8 ; j++)
		for (uint8_t i = 0 ; i < 40 ; i++)
			font_draw(i, j, ' ', FONT_NORMAL);
}


void
vt100_goto(
	uint8_t new_row,
	uint8_t new_col
)
{
	cur_row = new_row > 0 ? new_row - 1 : 0;
	cur_col = new_col > 0 ? new_col - 1 : 0;

	if (cur_row >= MAX_ROWS)
		cur_row = MAX_ROWS - 1;
	if (cur_col >= MAX_COLS)
		cur_col = MAX_COLS - 1;
}


static void
buzzer(void)
{
	// todo: use PWM on the buzzer
}


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
			vt100_clear();
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
				font_mod = FONT_NORMAL;
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
			vt100_clear();
			cur_row = cur_col = 0;
		} else
		if (c == 'K')
		{
			// <ESC>[K == erase to end of line
			for (uint8_t x = cur_col ; x < MAX_COLS ; x++)
				font_draw(x, cur_row, ' ', FONT_NORMAL);
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
			vt100_goto(arg1, arg2);
		} else
		if (c == 'm')
		{
			// <ESC>[{arg};{arg}m == set attributes
			// 0 == clear
			// no others are supported
			if (arg1 == 0)
				font_mod = FONT_NORMAL;

			if (arg2 == 1)
				font_mod |= FONT_UNDERLINE;
			else
			if (arg2 == 7)
				font_mod |= FONT_INVERSE;
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


void
vt100_putc(
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
	} else
 	if (c == '\xE')
	{
		// ^P or ASCII SHIFT-OUT (SO) switches sets, too
		// We do not support alternate char sets for now.
		// ignore.
	} else
	if (c == '\x8')
	{
		// erase the old char and backup
		font_draw(cur_col, cur_row, ' ', FONT_NORMAL);
		if (cur_col > 0)
		{
			cur_col--;
		} else {
			cur_col = 40;
			cur_row = (cur_row - 1 + MAX_ROWS) % MAX_ROWS;
		}
	} else {
		font_draw(
			cur_col,
			cur_row,
			c,
			font_mod
		);

		if (++cur_col == 40)
			goto new_row;
	}

	return;

new_row:
	cur_row = (cur_row + 1) % 8;
	cur_col = 0;
}
