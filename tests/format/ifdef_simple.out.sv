`ifdef FEATURE_A
    module feature_a (
        input  logic clk,
        output logic out
    );
        assign out = 1'b1;
    endmodule
`else
    module feature_a (
        input  logic clk,
        output logic out
    );
        assign out = 1'b0;
    endmodule
`endif
