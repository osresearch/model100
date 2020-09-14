`ifndef _pwm_v_
`define _pwm_v_


module pwm(
	input clk,
	input [WIDTH-1:0] duty,
	output out
);
	parameter WIDTH = 8;
	reg [WIDTH-1:0] counter;
	assign out = counter < duty;

	always @(posedge clk)
		counter <= counter + 1;
endmodule

`endif
