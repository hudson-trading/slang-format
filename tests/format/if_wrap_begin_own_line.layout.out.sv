// When an `if (...)` predicate wraps across multiple lines, the `begin`
// for the body goes on its own line at the parent indent rather than
// trailing the last line of the predicate. A short single-line predicate
// keeps `begin` on the header line.

module test;
    always_comb begin
        // Single-line predicate: `begin` stays inline.
        if (short_cond) begin
            a = b;
        end

        // Multi-line predicate: `begin` goes on its own line.
        if (some_long_condition_alpha || some_long_condition_beta
                || some_long_condition_gamma || some_long_condition_delta)
        begin
            x = y;
        end

        // `else if` with multi-line predicate also gets begin on own line.
        if (a) begin
            x = 1;
        end else if (another_long_condition_aaaa || another_long_condition_bbbb
                         || another_long_condition_cccc || another_long_condition_dddd)
        begin
            x = 2;
        end
    end
endmodule
