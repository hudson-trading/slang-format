// Regression: an empty `default: begin end` in a case statement reads as a "nothing
// block" and should stay on one line, not be split into
//     default: begin
//     end
// (Mirrors `if (a) begin end` already handled by end_else_inline.)

module top;
    always_comb begin
        case (opcode)
            OP_A: begin
                x = 1;
            end

            OP_B: begin
                x = 2;
            end

            default: begin end
        endcase
    end

    always_ff @(posedge clk) begin
        if (rst) begin
            pc_pending <= '0;
        end else if (en) begin
            unique case ({load, exec})
                2'b10: pc_pending <= '1;
                2'b01: pc_pending <= '0;
                default: begin end
            endcase
        end
    end
endmodule
