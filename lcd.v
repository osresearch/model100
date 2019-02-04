`ifndef _lcd_v_
`define _lcd_v_

/*
 * Draw bitmap data to the LCD display.
 *
 * Enable pin clocks data at the falling edge.
 *
 * This draws a row at a time in auto-advance mode.
 *
 * Each chip handles a 50x32 rectangle and is updated 8 pixel columns
 * at a time.  The column address auto-advances, so the address only
 * needs to be set at the start of each column.
 */

module lcd(
	input clk,
	input reset,

	// bitmap data to be displayed
	input [7:0] pixels,
	output reg [7:0] x, // 240 columns
	output reg [3:0] y, // 8 rows of 8 pixels
	output reg frame_strobe, // when starting a new frame

	// pins
	output reg [7:0] data_pin,
	output reg [LCD_MODULES-1:0] cs_pin,
	output reg cs1_pin,
	output reg rw_pin,
	output reg di_pin,
	output reg enable_pin,
	output reg reset_pin
);
	parameter LCD_MODULES = 10;

	assign rw_pin = 0; // always write

	localparam STATE_INIT	= 0;
	localparam STATE_RESET	= 1;
	localparam STATE_ON	= 2;
	localparam STATE_UP	= 3;
	localparam STATE_PAGE	= 4;
	localparam STATE_WAIT	= 5;
	localparam STATE_WAIT1	= 6;
	localparam STATE_WAIT2	= 7;
	localparam STATE_WAIT3	= 8;
	localparam STATE_DONE	= 9;
	localparam STATE_COORD	= 10;
	localparam STATE_COORD2 = 11;
	localparam STATE_WAIT4	= 12;
	localparam STATE_DATA	= 13;
	localparam STATE_DATA2  = 14;
	localparam STATE_DATA3  = 15;

	localparam MAX_X	= 240;
	localparam X_PER_MODULE	= 50;

	reg [24:0] counter;
	reg [3:0] state;
	reg [3:0] next_state;
	reg [6:0] disp_x;

	always @(posedge clk)
	begin
		counter <= counter + 1;
		frame_strobe <= 0;

		if (reset) begin
			state <= STATE_INIT;
			reset_pin <= 0; // negative logic
			cs_pin <= 0;
			cs1_pin <= 0;
			x <= 0;
			y <= 0;
			disp_x <= 0;
			enable_pin <= 1;
		end else
		if (counter[4:0] != 0) begin
			// do nothing... stretch the clocks
			// at 48 MHz this is a divide by 32, so roughly
			// a 1.5 MHz clock.
		end else
		case(state)
		STATE_INIT: begin
			reset_pin <= 1;
			state <= STATE_RESET;
			counter <= 1;
			enable_pin <= 1;
			x <= 0;
			y <= 0;
			disp_x <= 0;
			enable_pin <= 1;
		end
		STATE_RESET: begin
			// hold for a few ms
			cs1_pin <= 1;
			if (counter == 0)
				state <= STATE_ON;
			data_pin <= 0;
		end
		STATE_ON: begin
			di_pin <= 0;
			data_pin <= 8'b00111001; // "Turn on display"
			next_state <= STATE_UP;
			state <= STATE_WAIT;
		end
		STATE_UP: begin
			data_pin <= 8'b00111011; // "Up mode"
			next_state <= STATE_PAGE;
			state <= STATE_WAIT;
		end
		STATE_PAGE: begin
			data_pin <= 8'b00111110; // "Start page 0"
			next_state <= STATE_DONE;
			state <= STATE_WAIT;
		end
		STATE_DONE: begin
			state <= STATE_COORD;
		end

		/* Busy wait after a command and send to all modules */
		STATE_WAIT: begin
			enable_pin <= 1;
			cs_pin <= 1;
			state <= STATE_WAIT2;
		end
		STATE_WAIT2: begin
			// strobe the enable pin and wait a little while
			enable_pin <= 0;
			state <= STATE_WAIT3;
			//counter <= 1;
		end
		STATE_WAIT3: begin
			enable_pin <= 1;
			state <= STATE_WAIT4;
		end
		STATE_WAIT4: begin
			// select the next module
			cs_pin <= { cs_pin[9-1:0], cs_pin[9] };

			// have all have been selected?
			if (cs_pin[9])
				state <= next_state;
			else
				state <= STATE_WAIT2;
		end

		/* Framebuffer drawing code.
		 */
		STATE_COORD: begin
		 	// Send all the devices to the same row/column.
			// disp_x is ignored, since we always start at first
			di_pin <= 0;
			data_pin <= { y[1:0], 6'b000000 };
			enable_pin <= 1;
			next_state <= STATE_COORD2;
			state <= STATE_WAIT;

			if (y == 0)
				frame_strobe <= 1;
		end
		STATE_COORD2: begin
			// we start on the very first module after a
			// coordinate update.
			cs_pin <= 1;
			// data, not instruction
			di_pin <= 1;

			// return to the first column
			x <= 0;
			disp_x <= 0;

			enable_pin <= 1;
			state <= STATE_DATA;
		end

		STATE_DATA: begin
			enable_pin <= 1;
			data_pin <= pixels;
			state <= STATE_DATA2;
		end
		STATE_DATA2: begin
			// everything is stable, clock the modules
			enable_pin <= 0;
			state <= STATE_DATA3;
		end
		STATE_DATA3: begin
			enable_pin <= 1;

			// default is to select the next module
			// and continue to display the same column
			state <= STATE_DATA;
			cs_pin <= { cs_pin[9-1:0], cs_pin[9] };
			x <= x + X_PER_MODULE;

			if (cs_pin[4]) begin
				// end of first row of modules
				// move to the second row
				// rewind x to be back at the current
				// display column
				y[2] <= 1;
				x <= disp_x;
			end else
			if (cs_pin[9]) begin
				// end of the second row of modules
				// go back to the first row
				y[2] <= 0;

				if (disp_x == X_PER_MODULE-1) begin
					// move back to the first module,
					// on the next Y data page
					state <= STATE_COORD;
					y[1:0] <= y[1:0] + 1;
					x <= 0;
					disp_x <= 0;
				end else begin
					// advance the display X coordinate
					disp_x <= disp_x + 1;
					x <= disp_x + 1;
				end
			end
		end
		endcase
	end

endmodule


`endif
