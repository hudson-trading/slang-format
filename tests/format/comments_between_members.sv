module comments_between (
    input logic clk
);
    // Group A: status registers
    logic status_valid;
    logic status_ready;
    logic status_error;

    // Group B: data path
    logic [31:0] data_a;
    logic [31:0] data_b;

    /* Group C: control signals */
    logic ctrl_start;
    logic ctrl_stop;

    // Combinational
    always_comb begin
        // First assignment
        ctrl_start = status_valid & status_ready;

        // Second assignment
        ctrl_stop = status_error;
    end

    // Sequential
    always_ff @(posedge clk) begin
        /* Update data_a */
        data_a <= data_b;

        /* Update data_b */
        data_b <= data_a;
    end
endmodule
