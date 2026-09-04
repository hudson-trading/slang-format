// An inactive conditional branch containing only comments must remain in the
// formatter-owned trivia stream even though slang produces no disabled token.
module foo;
    always_comb begin
        `ifdef FEATURE
            // The inactive branch intentionally has no statement.
        `else
            signal_a = signal_b;
        `endif
    end
endmodule

module bar;
    `ifdef FEATURE
        logic signal_c;
        // Both trailing comments belong to the inactive branch.
        // Neither can disappear when the branch closes.
    `else
        logic signal_d;
    `endif
endmodule
