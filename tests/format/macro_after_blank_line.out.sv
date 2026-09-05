// Idempotency: when a comment + macro member follow a blank line, the
// formatter must not insert an extra blank line. Earlier the aligned
// emission path laid down an indent (` `*4`) at the start of the row
// and then the macro's sub-trivia processing emitted `\n` for the
// blank, leaving the indent as trailing whitespace. The next pass
// then read that whitespace-only line as an extra blank and the output
// would grow on every pass.

module foo;
    logic mod;

    // a comment
    `M(arg) x;
endmodule
