`ifndef OUTER
module column0_body (
    input logic clk,
    input logic rst_n,
`ifndef HAS_SIDEBAND
    input logic [3:0] sideband_in,
`endif
    output logic [7:0] out
);
    logic [7:0] val;
`ifndef INNER_A
`ifndef DEEP
    logic [7:0] reg_a;
`else
    logic [7:0] reg_b;
`endif
    assign out = val;
`elsif INNER_B
    assign out = 8'h01;
`else
    assign out = 8'h00;
`endif

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            val <= 8'd0;
        end else begin
`ifndef USE_FAST_PATH
            val <= val + 8'd4;
`else
            val <= val + 8'd1;
`endif
        end
    end
endmodule
`endif
