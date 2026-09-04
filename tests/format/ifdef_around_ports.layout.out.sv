module ifdef_ports (
    input logic clk,
    input logic rst_n,
    input logic [7:0] data_in,
    output logic [7:0] data_out
`ifdef HAS_SIDEBAND
    ,
    input  logic [3:0] sideband_in,
    output logic [3:0] sideband_out
`endif
);
    assign data_out = data_in;

    `ifdef HAS_SIDEBAND
        assign sideband_out = sideband_in;
    `endif
endmodule
