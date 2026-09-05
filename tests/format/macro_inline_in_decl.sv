// Macro invocation inside a data declaration. Slang's recovery splits
// `logic [7:0] x `MACRO();` into a DataDeclaration with a missing semi and a
// trailing EmptyMember whose real `;` carries the macro as trivia.
// The formatter must emit the macro followed immediately by the `;`, not
// drop the `;` or duplicate the macro.
module m;
    `ASSERT_PROLOGUE(clk, rst);

    `define HELPER_MACRO(A) \
    foo bar(.x(A))


    logic [7:0] reg_a `KEEP_ATTR(preserve);

endmodule
