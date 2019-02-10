`ifndef _spi_display_v_
`define _spi_display_v_

/**
 * SPI display interface.
 *
 * This emulates enough of the commands to pretend to be a ILI9340
 * that can connect to a Raspberry Pi and fill in a frame buffer.
 *
 * https://github.com/notro/fbtft/blob/master/fbtft-core.c
 *
 * Need to implement:
 * - set_addr_win (only operates on lines)
 * - write_vmem
 * - blank / power off?
 * - rotation commands?
 */

`ifdef 0
	write_reg(par, 0x28); /* display off */
	write_reg(par, 0x29); /* display on */
	set_addr(
	/* Column address set */
	write_reg(par, 0x2A,
		(xs >> 8) & 0xFF, xs & 0xFF, (xe >> 8) & 0xFF, xe & 0xFF);

	/* Row adress set */
	write_reg(par, 0x2B,
		(ys >> 8) & 0xFF, ys & 0xFF, (ye >> 8) & 0xFF, ye & 0xFF);

	/* Memory write */
	write_reg(par, 0x2C);
`endif

`include "spi_device.v"


module spi_display(
	input clk,
	output debug,
	output [7:0] uart_data,
	output uart_strobe,

	// physical interface
	input spi_clk,
	input spi_cs,
	input spi_di, // data in
	input spi_dc, // data / !command

	// outputs in spi_clk domain
	output reg [WIDTH-1:0] x,
	output reg [WIDTH-1:0] y,
	output reg strobe,
	output reg [15:0] pixels,

	output [WIDTH-1:0] x_start,
	output [WIDTH-1:0] y_start,
	output [WIDTH-1:0] x_end,
	output [WIDTH-1:0] y_end
);
	parameter WIDTH = 16;

	// input only SPI device
	wire [7:0] rx_data;
	wire rx_strobe;

	spi_device spi(
		.spi_clk(spi_clk),
		.spi_cs(spi_cs),
		.spi_mosi(spi_di),
		.spi_rx_strobe(rx_strobe),
		.spi_rx_data(rx_data)
	);

	reg [7:0] cmd;
	reg [2:0] cmd_bytes;

	reg [WIDTH-1:0] x_pos;
	reg [WIDTH-1:0] y_pos;
	reg [WIDTH-1:0] x_end = 1024-1;
	reg [WIDTH-1:0] y_end = 768-1;
	reg [WIDTH-1:0] x_start = 0;
	reg [WIDTH-1:0] y_start = 0;
	reg byte;

	always @(posedge spi_clk or posedge spi_cs)
	begin
		uart_strobe <= 0;
		strobe <= 0;
		debug <= 1;

		if (spi_cs) begin
			// no longer selected, reset our state
			byte <= 0;
		end else
		if (!rx_strobe) begin
			// nothing to do, wait for an incoming byte
		end else
		if (!spi_dc) begin
			// start of a new command, store the command id
			cmd <= rx_data;
			cmd_bytes <= 0;
			uart_strobe <= 1;
			uart_data <= rx_data;
		end else
		case(cmd)
		8'h2B: begin
			// row address
			case(bytes)
			0: x_start[WIDTH-1:8] <= rx_data;
			1: x_start[7:0] <= rx_data;
			2: x_end[7:0] <= rx_data;
			3: begin
				x_end[WIDTH-1:8] <= rx_data;
				x_pos <= x_start;
			end
			endcase
		end
		8'h2A: begin
			// column address
			case(bytes)
			0: y_start[WIDTH-1:8] <= rx_data;
			1: y_start[7:0] <= rx_data;
			2: y_end[WIDTH-1:8] <= rx_data;
			3: begin
				y_end[7:0] <= rx_data;
				y_pos <= y_start;
			end
			endcase
		end
		8'h2C: begin
			// write memory command
			if (bytes[0] == 0)
				pixels[15:8] <= rx_data;
			else begin
				pixels[7:0] <= rx_data;
				strobe <= 1;
				x <= x_pos;

				if (x_pos != x_end) begin
					// same line in the write region
					x_pos <= x_pos + 1;
				end else begin
					// wrap the write region
					x_pos <= x_start;
					if (y != y_end)
						y <= y + 1;
					else
						y <= y_start;
				end
			end
		end
		endcase
	end

endmodule
`endif
