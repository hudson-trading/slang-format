module ifdef_body (
    input logic clk,
    input logic rst_n,
    output logic [7:0] out
);
    logic [7:0] val;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            val <= 8'd0;
        end else begin
            `ifdef USE_FAST_PATH
            val <= val + 8'd4;
            `else
                val <= val + 8'd1;
            `endif
        end
    end

    `ifdef HAS_DEBUG
    // Debug probe
    logic [7:0] debug_val;
    assign debug_val = val;
    `endif

    assign out = val;
endmodule
