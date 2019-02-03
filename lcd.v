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
	localparam STATE_COORD3 = 12;
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
		if (counter[5:0] != 0) begin
			// do nothing... stretch the clocks
		end else
		case(state)
		STATE_INIT: begin
			reset_pin <= 1;
			state <= STATE_RESET;
			counter <= 1;
			enable_pin <= 1;
		end
		STATE_RESET: begin
			// hold for a few ms
			if (counter == 0)
				state <= STATE_ON;
			data_pin <= 0;
		end
		STATE_ON: begin
			cs1_pin <= 1;
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
			counter <= 1;
		end
		STATE_WAIT3: begin
			enable_pin <= 1;

			if (counter != 0) begin
				// wait for timeout
			end else begin
				// select the next module
				cs_pin <= { cs_pin[9-1:0], cs_pin[9] };

				// have all have been selected?
				if (cs_pin[9])
					state <= next_state;
				else
					state <= STATE_WAIT2;
			end
		end

		/* Framebuffer drawing code.
		 */
		STATE_COORD: begin
		 	// Send all the devices to the same row/column.
			// disp_x is ignored, since we always start at first
			cs_pin <= 1;
			di_pin <= 0;
			data_pin <= { y[1:0], 6'b000000 };
			enable_pin <= 1;
			next_state <= STATE_COORD3;
			state <= STATE_WAIT;
		end
		STATE_COORD2: begin
			enable_pin <= 0;
			state <= STATE_COORD3;
		end

		STATE_COORD3: begin
			// we start on the very first module after a
			// coordinate update.
			cs_pin <= 1;
			enable_pin <= 1;
			state <= STATE_DATA;
		end

		STATE_DATA: begin
			di_pin <= 1; // data, not instruction
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
			x <= x + 1;
			state <= STATE_DATA;

			// at the end of the line?
			if (x == MAX_X-1) begin
				// if we're on the first row of modules,
				// go to the second row.
				// if we're on the second row, go to the next
				// data page on the first row
				if (!y[2]) begin
					y <= { 1'b1, y[1:0] };
				end else begin
					y <= { 1'b0, y[1:0] + 2'b01 };
					state <= STATE_COORD;
				end

				// swap LCD modules
				cs_pin = { cs_pin[4:0], cs_pin[9:5] };
				disp_x <= 0;
				x <= 0;
			end else
			// at the end of this module? go to next module
			if (disp_x == X_PER_MODULE-1) begin
				disp_x <= 0;
				cs_pin = { cs_pin[9-1:0], cs_pin[9] };
			end
		end
		endcase
	end

endmodule


`endif
