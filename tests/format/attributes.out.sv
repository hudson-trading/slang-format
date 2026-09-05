(* synthesis *)
module attr_test (
    (* mark_debug = "true" *)
    input logic clk,
    (* mark_debug = "true" *)
    input logic rst_n,
    output logic [7:0] count
);
    (* keep = "true" *)
    logic [7:0] counter;

    (* full_case, parallel_case *)
    always_comb begin
        case (counter[1:0])
            2'b00: count = 8'd0;
            2'b01: count = 8'd1;
            2'b10: count = 8'd2;
            2'b11: count = 8'd3;
        endcase
    end

    always_ff @(posedge clk) begin
        if (!rst_n)
            counter <= '0;
        else
            counter <= counter + 1;
    end
endmodule
