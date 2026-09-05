// Regression: when a data/parameter
// declaration's RHS doesn't fit on the same line as the LHS, prefer breaking
// after `=` when that alone fits. If the RHS still needs inner breaks, compare
// the complete assignment and expression layouts by cost.
//
// Among inner break points, prefer the latest-evaluated operator (e.g.,
// the ternary's `?` over a comparison `>` inside the predicate).

package test_pkg;
    // Long ternary RHS — breaking at `=` puts the whole ternary on one
    // indented line, much cleaner than breaking inside the parens.
    localparam int handler_output_stage = (transport_pkg::identifier_width > 14) ? 1 : 0;

    // Long function-call RHS — break at `=` puts the call on the next
    // line at block indent, not the column-of-paren indent.
    parameter int handler_latency = getHandlerLatency(handler_input_stage, handler_match_stage);

    // Macro-arg call — should fit on one line or, if it must wrap,
    // wrap at `=` rather than splitting the function call.
    parameter int output_offset_width = $clog2(`MAX_OUTPUT_RECORD_BYTES);

    // This ternary does not fit after a break at `=` alone. Its expression
    // breaks compete with the assignment break, and prefer `?`/`:` over an
    // inner comparison.
    localparam int big_choice = (very_long_signal_name_alpha == very_long_signal_name_beta) ? some_longish_value_alpha + some_longish_value_beta : some_other_longish_value_gamma + some_other_longish_value_delta;
endpackage
