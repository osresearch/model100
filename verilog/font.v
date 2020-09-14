`ifndef _font_v_
`define _font_v_

module font_5x7(
	input clk,
	output reg [7:0] pixels,
	input [6:0] character,
	input [2:0] col
);
	// the font is stored as five bytes each and only stores
	// the characters from space (0x20) to ~ (0x7E)
	// to fit in a single 4KB block RAM
	reg [7:0] font[8'h60 * 5:0];
	initial $readmemh("font.hex", font);

	// select the character and column from the font RAM
	wire [6:0] char = character - 7'h20;
	wire [8:0] font_col = char * 5 + col;

	always @(posedge clk)
	begin
		pixels = col == 5 ? 8'h0 : font[font_col];
	end
endmodule

`endif
