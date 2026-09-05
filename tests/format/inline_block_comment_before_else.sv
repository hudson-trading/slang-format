// A block comment between end and else remains inline and idempotent.
module m;
    always_comb begin
        if (push & pop) begin end /* tool coverage_on */
        else if (push) begin
            value = next_value;
        end
    end
endmodule
