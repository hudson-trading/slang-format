// Regression: comparisons (==, !=, <, <=, >, >=) are
// normally atomic, but should be allowed as break points when a side is
// a parenthesized sub-expression containing a deeper binary op. Without
// this, the formatter would recurse into the parens and break at a
// *higher*-precedence inner op (e.g. `-`, `+`) — which inverts the
// natural precedence ordering that the syntax tree encodes.

module test;
    // RHS is a paren containing `-`. The member DP prefers the outer
    // assignment break and keeps the comparison atomic.
    always_ff @(posedge clk) begin
        event_header_offset_last <=
            (event_header_offset_next == (descriptor.event_header_offset - 'd1));
    end

    // Negative case: both sides are simple — no deeper binary. The
    // comparison stays atomic; line overflows instead.
    always_ff @(posedge clk) begin
        match_result[i] =
            lookup_response.data.entry.slots[i].valid
            && lookup_response.data.entry.slots[i].reference == request_input.current_reference;
    end
endmodule
