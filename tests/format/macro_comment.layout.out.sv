`define PIPELINE(sig, stages)  /* pipeline macro */

module macro_comment (
    input logic clk,
    input logic [7:0] data_in,
    output logic [7:0] data_out
);
    // Pipeline stage comment
    `PIPELINE(data_in, 2)
    `PIPELINE(data_out, 3)

    `PIPELINE(data_in, 2);
    `PIPELINE(data_out, 3);

    // Block comment before macro
    /* setup */
    `PIPELINE(data_in, 1)

    logic [7:0] result;
    assign result = data_in;
endmodule
