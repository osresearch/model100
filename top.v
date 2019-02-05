`include "pwm.v"
`include "lcd.v"
`include "uart.v"
`include "font.v"
`include "textbuffer.v"

module top(
	output serial_txd,
	input serial_rxd,
	output spi_cs,
	output led_r,

	output gpio_2,
	output gpio_23,
	output gpio_25,
	output gpio_26,
	output gpio_27,
	output gpio_32,
	output gpio_35,
	output gpio_31,
	output gpio_37,
	output gpio_38,
	output gpio_28,
	output gpio_4,
	output gpio_44,
	output gpio_6,
	output gpio_9,
	output gpio_11,
	output gpio_18,
	output gpio_19,
	output gpio_13,
	output gpio_21,
	output gpio_12,
	output gpio_3,
	output gpio_48,
	output gpio_45,
	output gpio_47,
	output gpio_46
);

	wire clk_48mhz;
	wire reset = 0;
	SB_HFOSC ocs(1,1,clk_48mhz);
	wire clk = clk_48mhz;

	wire lcd_frame_strobe;
	wire [7:0] lcd_x;
	wire [2:0] lcd_y;
	wire [2:0] lcd_subcol;
	wire [7:0] lcd_char;

	reg [63:0] framebuffer[239:0];
	initial $readmemh("fb.hex", framebuffer);

	textbuffer tb(
		.clk(clk),
		.char(lcd_char),
		.subcol(lcd_subcol),
		.lcd_x(lcd_x),
		.lcd_y(lcd_y),
		.write_strobe(0)
	);

	wire inverted_video = lcd_char[7];
	wire [7:0] pixels;

	font_5x7 font(
		.clk(clk),
		.pixels(pixels),
		.character(lcd_char[6:0]),
		.col(lcd_subcol)
	);

/*
	wire [63:0] fb = framebuffer[lcd_x];
	wire [7:0] pixels = {
		fb[{lcd_y, 3'h7}],
		fb[{lcd_y, 3'h6}],
		fb[{lcd_y, 3'h5}],
		fb[{lcd_y, 3'h4}],
		fb[{lcd_y, 3'h3}],
		fb[{lcd_y, 3'h2}],
		fb[{lcd_y, 3'h1}],
		fb[{lcd_y, 3'h0}]
	};
	//wire [7:0] pixels = lcd_x;
*/

	wire [7:0] lcd_data = {
		gpio_37,
		gpio_31,
		gpio_35,
		gpio_32,
		gpio_27,
		gpio_26,
		gpio_25,
		gpio_23
	};

	// pinout on the cable is 4, 3, 9, 2, 8, 1, 7, 0, 6, 5
	wire [9:0] lcd_cs = {
		gpio_13, // 9
		gpio_18, // 8
		gpio_9, // 7
		gpio_44, // 6
		gpio_4, // 5
		gpio_12, // 4
		gpio_21, // 3
		gpio_19, // 2
		gpio_11, // 1
		gpio_6 // 0
	};

	wire lcd_reset = gpio_3; // can be ignored, pull high
	wire lcd_cs1 = gpio_48;
	wire lcd_enable = gpio_45;
	wire lcd_rw = gpio_47; // can be ignored, pull low
	wire lcd_di = gpio_46;

	lcd modell100_lcd(
		.clk(clk),
		.reset(reset),
		.pixels(inverted_video ? ~pixels : pixels),
		.x(lcd_x),
		.y(lcd_y),
		.frame_strobe(lcd_frame_strobe),
		.data_pin(lcd_data),
		.cs_pin(lcd_cs),
		.cs1_pin(lcd_cs1),
		.rw_pin(lcd_rw),
		.di_pin(lcd_di),
		.enable_pin(lcd_enable),
		.reset_pin(lcd_reset)
	);

	reg [28:0] dim;
	always @(posedge clk) dim <= dim + 1;

	// generate a 1/4 duty cycle wave for the
	// negative voltage charge pump circuit
	pwm negative_charge_pump(
		.clk(clk),
		//.duty(dim[28:21]),
		.duty(128),
		.out(gpio_28)
	);

	// contrast display at 1/2 duty cycle
	pwm contrast(
		.clk(clk),
		.duty(dim[28:21]),
		//.duty(255),
		.out(gpio_38)
	);

/*
	// clk == 48 MHz
	// buzz the gpio_2
	reg [15:0] music[7:0];
	initial begin
		music[0] <= { 7'h7F, 9'd500 };
		music[1] <= { 7'h40, 9'd00 };
		music[2] <= { 7'h7F, 9'd250 };
		music[3] <= { 7'h7F, 9'd00 };
		music[4] <= { 7'h7F, 9'd400 };
		music[5] <= { 7'h40, 9'd00 };
		music[6] <= { 7'h7F, 9'd200 };
		music[7] <= { 7'h7F, 9'd00 };
	end
	reg [2:0] pos;
	reg [25:0] delay;
	reg [31:0] div;

	assign gpio_2 = div[27];

	always @(posedge clk)
	begin
		div <= div + music[pos][8:0];
		delay <= delay + 1;

		if (delay[25-:7] == music[pos][15:9])
		begin
			pos <= pos + 1;
			delay <= 0;
		end
	end
*/

	// read bytes from the serial port for the framebuffer
	assign spi_cs = 1; // it is necessary to turn off the SPI flash chip
	assign serial_txd = 1;

	// generate a 3 MHz/12 MHz serial clock from the 48 MHz clock
	// this is the 3 Mb/s maximum supported by the FTDI chip
	wire clk_1, clk_4;
	divide_by_n #(.N(16)) div1(clk, reset, clk_1);
	divide_by_n #(.N( 4)) div4(clk, reset, clk_4);

	wire [7:0] uart_rxd;
	wire uart_rxd_strobe;
	reg [7:0] x;
	reg [2:0] y;
	wire [63:0] wr_fb = framebuffer[x];
	wire [7:0] wr_pixels = {
		wr_fb[{y, 3'h7}],
		wr_fb[{y, 3'h6}],
		wr_fb[{y, 3'h5}],
		wr_fb[{y, 3'h4}],
		wr_fb[{y, 3'h3}],
		wr_fb[{y, 3'h2}],
		wr_fb[{y, 3'h1}],
		wr_fb[{y, 3'h0}]
	};

	uart_rx rxd(
		.mclk(clk),
		.reset(reset),
		.baud_x4(clk_4),
		.serial(serial_rxd),
		.data(uart_rxd),
		.data_strobe(uart_rxd_strobe)
	);

	always @(posedge clk)
	begin
		led_r <= 1;
		if (uart_rxd_strobe) begin
			led_r <= 0;
			framebuffer[x][8*y +: 8] <= {
				uart_rxd[0],
				uart_rxd[1],
				uart_rxd[2],
				uart_rxd[3],
				uart_rxd[4],
				uart_rxd[5],
				uart_rxd[6],
				uart_rxd[7]
			};

			y <= y + 1;
			if (y == 7) begin
				if (x == 239)
					x <= 0;
				else
					x <= x + 1;
			end
		end
	end

endmodule
