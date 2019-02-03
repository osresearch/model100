`include "pwm.v"
`include "lcd.v"

module top(
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
	SB_HFOSC ocs(1,1,clk_48mhz);
	wire clk = clk_48mhz;

	wire [7:0] lcd_x;
	wire [3:0] lcd_y;
	reg [63:0] framebuffer[239:0];
	initial $readmemh("fb.hex", framebuffer);

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

	wire lcd_reset = gpio_3;
	wire lcd_cs1 = gpio_48;
	wire lcd_enable = gpio_45;
	wire lcd_rw = gpio_47;
	wire lcd_di = gpio_46;

	lcd modell100_lcd(
		.clk(clk),
		.reset(0),
		.pixels(pixels),
		.x(lcd_x),
		.y(lcd_y),
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
	assign gpio_28 = dim[10];
/*
	pwm negative_charge_pump(
		.clk(clk),
		.duty(dim[28:21]),
		.out(gpio_28)
	);
*/

	// contrast display at 1/2 duty cycle
	pwm contrast(
		.clk(clk),
		//.duty(dim[21+:8]),
		.duty(255),
		.out(gpio_38)
	);
endmodule
