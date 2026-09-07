// Macro directives follow their enclosing scope, while body bytes stay intact.
// Continuation whitespace can contribute to strings after macro expansion.

        `define TOP_MACRO(x) \
            do_a(x); \
              do_b(x);

module dut;
`define INNER_MACRO(y) \
    inner_call_a(y); \
      inner_call_b(y);
endmodule
