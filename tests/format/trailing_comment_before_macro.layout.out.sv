// Trailing comments survive when the next port starts with a freestanding
// macro directive. Slang's recovery wraps the macro into a phantom
// ImplicitAnsiPort whose first-token trivia contains both the macro and
// the *previous* port's trailing comment. The formatter must still emit
// the comment inline on the previous port's line rather than promoting it
// to a standalone LineComment entry.

module dut (
    input pkg_x::time_t ev_time_a,  // first comment stays inline
    input pkg_x::time_t ev_time_b,  // second comment stays inline too

    `SOME_MACRO
    input wire wd_tripped
);
endmodule
