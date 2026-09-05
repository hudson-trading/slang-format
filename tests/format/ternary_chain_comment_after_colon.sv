// Regression: a line comment between a ternary segment's `:` and the
// next segment's predicate must be preserved (was dropped because the
// chain emitter sets skipNextLeadingTrivia for the next predicate's
// first token). Covers both "trailing on `:` line" comments and
// "standalone between segments" comments.

module test;
    always_comb begin
        // Trailing-on-`:`-line comment.
        out_a = (!barrel) ? word_t'(shift_const) :  // sets either 0 or 1 even when word size > 1
                data[(i + distance + data_out_width) % data_out_width];

        // Standalone-between-segments comment.
        out_b = message_has_object ?
                    // send data message
                    16'(hdr_bytes + obj_blocks*8) :
                    // resend message
                    16'(hdr_bytes + resend_info*8);
    end
endmodule
