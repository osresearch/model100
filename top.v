`include "pwm.v"
`include "lcd.v"
`include "uart.v"
`include "font.v"
`include "textbuffer.v"
`include "spi_display.v"

module top(
	output serial_txd,
	input serial_rxd,
	output spi_flash_cs,
	output led_r,
	output led_g,
	output led_b,

	// LCD data and keyboard rows are shared
	inout gpio_37,
	inout gpio_31,
	inout gpio_35,
	inout gpio_32,
	inout gpio_27,
	inout gpio_26,
	inout gpio_25,
	inout gpio_23,

	// LCD control signals
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
	//output gpio_3, // LCD
	output gpio_48,
	output gpio_45,
	//output gpio_47, // LCD
	output gpio_46,

	// spi display from the pi (should be the keyboard)
	input gpio_34,
	input gpio_43,
	input gpio_36,
	input gpio_42,
	input gpio_38,

	// for the keyboard colums, eventually
	output gpio_2,
	output gpio_47,
	output gpio_3
);

	wire clk_48mhz;
	wire reset = 0;
	SB_HFOSC ocs(1,1,clk_48mhz);
	wire clk = clk_48mhz;

	reg lcd_frame_strobe = 1;
	wire [7:0] lcd_x;
	wire [2:0] lcd_y;
	wire [2:0] lcd_subcol;
	wire [7:0] lcd_char;

	reg [63:0] framebuffer[239:0];
	initial $readmemh("fb.hex", framebuffer);

`ifdef TEXT_MODE
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
`else

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
`endif

	wire [7:0] lcd_data;

	// shared by LCD and keyboard
	wire [7:0] data_pins = {
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

	wire lcd_reset; // = gpio_3; // can be ignored, pull high
	wire lcd_cs1 = gpio_48;
	wire lcd_enable = gpio_45;
	wire lcd_rw; // = gpio_47; // can be ignored, pull low
	wire lcd_di = gpio_46;

	lcd modell100_lcd(
		.clk(clk),
		.reset(reset),
		//.pixels(inverted_video ? ~pixels : pixels),
		.pixels(pixels),
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

/*
	// contrast display at 1/2 duty cycle
	pwm contrast(
		.clk(clk),
		.duty(dim[28:21]),
		//.duty(255),
		.out(gpio_38)
	);

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
	assign spi_flash_cs = 1; // it is necessary to turn off the SPI flash chip

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

	reg uart_txd_strobe;
	reg uart_txd_ready;
	reg [7:0] uart_txd_data;

	uart_tx txd(
		.mclk(clk),
		.reset(reset),
		.baud_x1(clk_1),
		.serial(serial_txd),
		.ready(uart_txd_ready),
		.data(uart_txd_data),
		.data_strobe(uart_txd_strobe)
	);

	uart_rx rxd(
		.mclk(clk),
		.reset(reset),
		.baud_x4(clk_4),
		.serial(serial_rxd),
		.data(uart_rxd),
		.data_strobe(uart_rxd_strobe)
	);

	reg lcd_output = 1;
	wire [7:0] key_row;
	wire [7:0] key_col_pin = {
		gpio_34,
		gpio_43,
		gpio_36,
		gpio_42,
		gpio_38,
		gpio_2,
		gpio_47,
		gpio_3
	};
	SB_IO #(
		.PIN_TYPE(6'b1010_01), // tristable
		.PULLUP(1'b 1)
	) key_row_buffer[7:0](
		.OUTPUT_ENABLE(lcd_output),
 		.PACKAGE_PIN(data_pins),
		.D_IN_0(key_row),
		.D_OUT_0(lcd_data)
	);

/*
	reg [18:0] counter;
	reg [3:0] col;
	reg [7:0] col_driver = 8'b 1111_1110;
	assign key_col_pin = col_driver;

	always @(posedge clk)
	begin
		uart_txd_strobe <= 0;
		counter <= counter + 1;

		if (counter == 0) begin
			// stop the LCD task, wait for it to end
			lcd_frame_strobe <= 0;
		end else
		if (counter == 1000) begin
			lcd_output <= 0;
			uart_txd_strobe <= 1;
			uart_txd <= col;
			col_driver <= ~(1 << col);
			//key_col_pin <= ~(1 << col);
		end else
		if (counter == 1500) begin
			uart_txd <= ~key_row;
			uart_txd_strobe <= 1;
			//col_driver <= { col_driver[7:0], col_driver[8] };

			if (col == 8) begin
				col <= 0;
			end else begin
				col <= col + 1;
			end

			// re-enable the LCD output
			lcd_frame_strobe <= 1;
			lcd_output <= 1;
		end
		
	end
*/

`ifdef UART_FB
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
`endif

	// interface with the Raspiberry Pi SPI TFT library
	wire spi_tft_strobe;
	wire [15:0] spi_tft_pixels;
	wire [15:0] spi_tft_x;
	wire [15:0] spi_tft_y;
	wire [4:0] spi_tft_r = spi_tft_pixels[15:11];
	wire [5:0] spi_tft_g = spi_tft_pixels[10:5];
	wire [4:0] spi_tft_b = spi_tft_pixels[4:0];

	// 240 x 64
/*
	wire [63:0] spi_framebuffer_col = framebuffer[spi_tft_x[7:0]];
	wire spi_frame_buffer_pixel = spi_framebuffer_col[spi_tft_y[5:0]];
*/
	wire [63:0] spi_framebuffer_col = framebuffer[128];
	wire spi_frame_buffer_pixel = spi_framebuffer_col[spi_tft_y[5:0]];

	wire spi_dc = gpio_43;
	wire spi_di = gpio_36;
	wire spi_clk = gpio_42;
	wire spi_cs = gpio_38;

	wire [16-1:0] x_end;
	wire [16-1:0] y_end;
	wire [16-1:0] x_start;
	wire [16-1:0] y_start;

	reg [18:0] out_count;
	always @(posedge clk) begin
		uart_txd_strobe <= 0;

		if (uart_txd_ready
		&& !uart_txd_strobe) begin
			out_count <= out_count + 1;

			if (out_count[14:0] == 0) begin
			uart_txd_strobe <= 1;

			case (out_count[18:15])
			0: uart_txd_data <= x_start[15:8];
			1: uart_txd_data <= x_start[7:0];
			2: uart_txd_data <= x_end[15:8];
			3: uart_txd_data <= x_end[7:0];
			4: uart_txd_data <= y_start[15:8];
			5: uart_txd_data <= y_start[7:0];
			6: uart_txd_data <= y_end[15:8];
			7: uart_txd_data <= y_end[7:0];
			8: uart_txd_data <= 8'hFF;
			9: uart_txd_data <= 8'hFF;
			10: uart_txd_data <= 8'hFF;
			11: uart_txd_data <= 8'hff;
			12: uart_txd_data <= 8'h01;
			13: uart_txd_data <= 8'h02;
			14: uart_txd_data <= 8'h03;
			15: uart_txd_data <= 8'h04;
			endcase
			end
		end
	end

	spi_display spi_display_inst(
		.debug(led_g),
		.x_start(x_start),
		.y_start(y_start),
		.x_end(x_end),
		.y_end(y_end),

		//.uart_strobe(uart_txd_strobe),
		//.uart_data(uart_txd_data),

		// physical interface
		.spi_clk(spi_clk),
		.spi_dc(spi_dc),
		.spi_di(spi_di),
		.spi_cs(spi_cs),

		// output
		.pixels(spi_tft_pixels),
		.strobe(spi_tft_strobe),
		.x(spi_tft_x),
		.y(spi_tft_y)
	);

	always @(posedge spi_clk)
	begin
		led_r <= 1;
		//led_g <= 1;
		led_b <= 1;

		if (spi_tft_strobe)
		begin
			//led_r <= 0;

			// convert to monochrome if any of the top bits
			// of the RGB pixel are set
			if (spi_tft_x < 240 && spi_tft_y < 64)
			framebuffer[spi_tft_x[7:0]][spi_tft_y[5:0]] <=
				spi_tft_r[4] | spi_tft_g[5] | spi_tft_b[4];
		end
	end

endmodule
