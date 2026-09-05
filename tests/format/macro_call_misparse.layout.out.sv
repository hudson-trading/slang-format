// Macros like `MACRO(args) = expr;` at module body confuse slang's parser:
// it sees `MACRO(args);` as a no-name module instantiation, then splits
// `= expr;` into 2 more error-recovery members. The formatter must NOT
// inflict its module-instance formatting (space before `(`, vertical
// connection wrap) on that misparse — instead, emit the macro call as a
// flat function-call-style line so the user's source is preserved.

module top;
    CUSTOM_ASSIGN(pkg_x::message_hdr_t, oob_event_hdr) = pkg_x::message_hdr_t'(oob_event_out.data);

    // Multi-arg macro without RHS (parses as no-decl instance with multiple
    // ordered connections). Should still stay on one line.
    SOME_MACRO(arg_one, arg_two, arg_three);

    // Real module instance (has a decl name) — preserves space before `(`
    // and standard formatting.
    real_module
      u_inst (.clk(clk), .rst(rst));
endmodule
