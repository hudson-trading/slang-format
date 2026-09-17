// Regression: comparison
// operators (==, !=, <, <=, >, >=) should be treated as atomic in
// wrapping decisions — the formatter must not split at a comparison
// op if a lower-precedence operator (||, &&, ?:) is a viable break.
// If the comparison is the ONLY available break, prefer to overflow
// rather than split inside it.

package test_pkg;
    function automatic logic check_match();
        for (int i = 0; i < cache_size; i++) begin
            // The && offers a viable break point. The comparison (==) on
            // the right side of the && must stay on one line — never split
            // `reference == current_reference` even if it overflows.
            match_result[i] =
                lookup_response.data.entry.slots[i].valid
                && lookup_response.data.entry.slots[i].reference == request_input.current_reference;
        end
    endfunction

    // A relational op (>) inside a parenthesized predicate must not be
    // broken when the outer ternary or && already provides a break.
    function automatic logic check_size();
        return ((sample.extended.count == 0)
                    || (sample.extended.count > previous_state.count)) ? clamped_a : clamped_b;
    endfunction
endpackage
