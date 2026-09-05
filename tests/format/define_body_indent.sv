// `define body lines shift past the directive by one indent level as a
// block — relative indents among body lines are preserved (so a line that
// was extra-indented in source stays extra-indented in the output). For
// `define inside a module, the directive's own indent is the module body
// indent and the body lands one further indent past that. Idempotent: the
// min source column is re-anchored to bodyBase, so re-formatting the
// already-indented output produces the same result.

        `define TOP_MACRO(x) \
            do_a(x); \
              do_b(x); \
            do_c(x);

module dut;
`define INNER_MACRO(y) \
    inner_call_a(y); \
      inner_call_b(y); \
    inner_call_c(y);
endmodule
