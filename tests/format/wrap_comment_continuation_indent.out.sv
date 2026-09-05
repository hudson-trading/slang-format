// Regression: when an assignment-style RHS
// contains an internal line comment that forces a break at an inner
// binary operator, the formatter must also break at `=` so the inner
// break lands at the proper continuation indent (aligned past `=` or
// at block + 1), not at the parent's block indent.
//
// Before this fix the second operand landed at the parameter's own
// indent column, looking like it had been dedented out of the
// expression.

package test_pkg;
    // Internal trailing comment after `+` — should align continuation to
    // column-after-`=`, not parent block indent.
    parameter int num_in_flight_events = 2 +  // Input pipe with double buffer
                                         2;  // Decode processing pipeline

    // Same shape with `&&` — RHS is a binary chain with comments.
    parameter bit cond = a_flag &&  // first criterion
                         b_flag;  // second criterion

    // A sole-RHS call made multiline by argument comments moves below `=`,
    // with its argument list nested beneath the call.
    initial begin
        result =
            compute_hash(
                input_key,  // key
                hash_seed,  // seed
                num_rounds  // rounds
            );
    end
endpackage
