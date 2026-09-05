`ifdef OUTER
module nested_ifdef (
    input logic clk,
    output logic [7:0] data
);
    `ifdef INNER_A
    logic [7:0] reg_a;
    assign data = reg_a;
    `elsif INNER_B
    logic [7:0] reg_b;
    assign data = reg_b;
    `else
    assign data = 8'h00;
    `endif
endmodule
`endif
