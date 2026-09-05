// An unexpanded macro used as a prefix to a declaration (`MACRO logic x;`,
// all on one source line) is parsed by slang's error recovery as a
// placeholder declaration that owns a parser-inserted `;`, followed by the
// real declaration. Splitting the macro onto its own line instead reparses as
// a bare EmptyMember with no `;` — a different CST. So the formatter must keep
// an inline prefix-macro on the declaration's line; otherwise the round-trip
// drops a token and the CST validator (rightly) flags a mismatch.
//
// Macro-prefixed rows do NOT participate in column alignment: the macro text
// would leak into the type column and the per-row whitespace after the macro
// varies in source. They emit with normalized single-space gaps instead. The
// plain (non-macro) declarations still align with each other.
module test;
    logic                plain_a;
    logic [WIDTH*16-1:0] plain_b;
    `MAYBE_UNUSED logic err_a;
    `MAYBE_UNUSED logic err_b;
    // Named-type prefix-macro: source had irregular spacing after the macro
    // (one space here, several there); output normalizes to a single space.
    `MAYBE_UNUSED pkg::some_type_t err_c;
    `MAYBE_UNUSED pkg::some_type_t err_d;
endmodule
