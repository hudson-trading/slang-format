module top (
    input logic clk,
    input logic rst_n,
    output logic [7:0] data
);
    logic [7:0] counter;
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n)
            counter <= 8'd0;
        else
            counter <= counter + 1;
    end
    assign data = counter;
endmodule
