// Regression: a chain of nested ternaries
// (false branch is itself a ConditionalExpression) is formatted as an
// aligned table. Each line is `cond ? value :` with `:` trailing;
// conditions are padded so all `?` operators line up. The final-else value
// sits on its own line at the value column. Reads as an if-else-if cascade.
//
// The first row stays after the assignment when it fits; otherwise the
// table starts at the normal continuation indent.

package test_pkg;
    // Chain of 2 ternaries with parenthesized predicates and short trues.
    localparam auxiliary_divider = (input_multiplier > maximum_second_threshold_value) ? 3 : (input_multiplier > maximum_first_threshold_value) ? 2 : 1;

    // Longer chain — 3 ternaries.
    localparam clock_div_select = (input_freq > max_freq_threshold_value_a) ? 4 : (input_freq > max_freq_threshold_value_b) ? 3 : (input_freq > max_freq_threshold_value_c) ? 2 : 1;

    // Short chain that fits inline — no wrap at all.
    localparam tiny = (a) ? 1 : (b) ? 2 : 0;
endpackage

module test;
    // Continuous assign with chain.
    assign mode = (cfg_mode_select_signal == MODE_FAST_PATH) ? 1 : (cfg_mode_select_signal == MODE_SLOW_PATH) ? 2 : 0;
endmodule
