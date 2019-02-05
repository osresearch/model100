`ifndef _textbuffer_v_
`define _textbuffer_v_

module textbuffer(
	input clk,

	// read out
	output reg [7:0] char,
	output reg [2:0] subcol,
	input [7:0] lcd_x, // 0 - 239 pixels
	input [2:0] lcd_y,  // 0 - 7 lines

	// write update
	input write_strobe,
	input [7:0] in,
	input [4:0] write_x, // 0-39 column
	input [2:0] write_y  // 0 - 7 lines
);

	// the text buffer is 40x8 on screen, but stored 64x8
	reg [7:0] text[64*8-1:0];
	initial $readmemh("text.hex", text);

	// cheaper than a divide by six operation
	reg [7:0] div6[255:0];
	integer col;
	initial begin
		for(col = 0 ; col < 256 ; col++)
			div6[col] <= col / 6;
	end

	reg [5:0] lcd_column;
	reg [2:0] lcd_subcol;
	wire [8:0] lcd_pos = { lcd_y, lcd_column };
	assign char = text[lcd_pos];
	assign subcol = lcd_subcol;

	wire [8:0] write_pos = { write_y, write_x };

	always @(posedge clk)
	begin
		// compute the column and sub column of the x position
		lcd_column <= div6[lcd_x];
		lcd_subcol <= lcd_x - (div6[lcd_x] * 6);

		// update the text at the write position
		if (write_strobe)
			text[write_pos] <= in;
	end
endmodule
