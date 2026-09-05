// Top-level file comment
// spanning multiple lines
module comments_test (
    input logic clk, // clock signal
    input logic rst_n, // active-low reset
    output logic [7:0] data // output bus
);
    // Signal declarations
    logic [7:0] counter; // 8-bit counter
    logic enable; // enable signal

    // Sequential block
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            counter <= 8'd0; // reset counter
            // could also use '0
        end
        else if (enable) begin
            counter <= counter + 1; // increment
        end
    end

    // Drive output
    assign data = counter;
    // End of module
endmodule
