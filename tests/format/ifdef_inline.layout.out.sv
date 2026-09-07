module inline_ifdef (
    input logic clk,
    input logic rst,
    output logic [7:0] data
);
    logic clk_gated;

    // Inline ifdef inside port connection
    clk_gate
      u_gate (
        .clk_in(clk),
        .clk_out(`ifndef DISABLE_GATING clk_gated `endif),
        .enable(rst)
    );

    // Block-level ifdef inside a parameter (should stay block-level)
    parameter int DEPTH =
    `ifdef CUSTOM_DEPTH
        `CUSTOM_DEPTH;
    `else
        16;
    `endif

    // Inline ifdef with else
    assign data = `ifdef USE_ALT_DATA 8'hFF `else 8'h00 `endif ;

    logic [DEPTH-1:0] mem;
endmodule
