// A leading blank line in an inactive conditional branch must remain idempotent.
module foo;
    `ifdef FEATURE

        logic signal_a;

        always_ff @(posedge clk) begin
            signal_a <= signal_b;
        end
    `endif
endmodule
