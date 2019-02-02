`include "pwm.v"

module top(
	output gpio_28
);

	wire clk_48mhz;
	SB_HFOSC ocs(1,1,clk_48mhz);
	wire clk = clk_48mhz;

	// generate a 1/4 duty cycle wave for the
	// negative voltage charge pump circuit

	pwm negative_charge_pump(
		.clk(clk),
		.duty(64),
		.out(gpio_28)
	);
endmodule
