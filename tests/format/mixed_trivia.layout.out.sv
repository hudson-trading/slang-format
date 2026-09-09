// Line comment before ifdef
`ifndef SIM
    /* Block comment inside ifdef */
    module mixed_trivia (
        input logic clk,  // clock
        input logic rst_n /* reset */
    );
        // Comment before ifdef inside module
        `ifdef DEBUG
            // Nested comment in ifdef
            logic debug_sig;
        `else
            /* Block comment in else branch */
            logic prod_sig;
        `endif

        /* Multi-line block comment
     * before always block
     */
        always_ff @(posedge clk) begin
            // Line comment in procedural block
            `ifdef DEBUG
                debug_sig <= 1'b0; // inline comment in ifdef
            `endif
        end
    endmodule
`endif  // SIM
