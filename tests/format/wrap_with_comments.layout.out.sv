module wrap_with_comments;
    // Comment preserved after = when expression wraps
    wire foo =  // trailing comment after equals
               (a && b && c) || (d && e && f);

    always_comb foo =  // trailing comment after equals
                      (a && b && c) || (d && e && f);

    // Comment between operands in a wrapping binary chain
    wire bar = (very_long_signal_name_a && very_long_signal_name_b)
               ||  // comment between operands
               (very_long_signal_name_c && very_long_signal_name_d);

    // Wrap without comments (baseline)
    wire baz = (very_long_signal_name_a && very_long_signal_name_b)
                   || (very_long_signal_name_c && very_long_signal_name_d);

    wire foo =  // trailing comment after equals
               // trailing comment line
               (a && b && c) || (d && e && f)  // trailing comment expr
    ;  // trailing comment semicolon

    always_ff @(posedge clk) aaaaaaaaa <=  // comment0
        bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb  // comment1
            ? cccccccccccccccccccccccccccccccccccccccccccccccc  // comment2
            :  // comment3
            ddddddddddddddddddddddddddddddddddddddddddd;  // comment4

    // Inline-fitting expression with trailing comments after each operand:
    // continuation lines must align to the expression start (after '='),
    // not to the member's block indent.
    localparam int total_count = 11 * 2  // group a
                                 + 13  // group b
                                 + 2  // group c
                                 + 1;  // group d

endmodule
