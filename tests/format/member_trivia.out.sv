module member_trivia;
    // Group A: inputs
    logic clk;
    logic rst_n;
    logic valid;

    // Group B: outputs
    logic [7:0] data_out;
    logic       ready;

    /* State machine registers */
    logic [1:0] state;
    logic [1:0] next_state;

    /*
     * Multi-line block comment
     * between member groups
     */
    always_ff @(posedge clk) begin
        if (!rst_n)
            state <= 2'b00;
        else
            state <= next_state;
    end

    /* Combinational next-state logic */
    always_comb begin
        /* Default: hold state */
        next_state = state;

        /* Transitions */
        case (state)
            2'b00:   if (valid)
                next_state = 2'b01;
            2'b01:   next_state = 2'b10;
            default: next_state = 2'b00;
        endcase
    end

    // Output logic
    assign data_out = (state == 2'b01) ? 8'hFF : 8'h00;
    assign ready = (state == 2'b00);
endmodule
