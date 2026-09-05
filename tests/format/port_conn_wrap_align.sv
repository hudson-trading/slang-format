// When a port-connection expression splits vertically, its body should
// hang off the bracket column — NOT jump back to the outer member indent.
// This applies to concatenations, struct patterns, and ordinary
// argument lists alike.

module top;
  ParamRam #(
    .doc({
      "first row of explanatory text describing the slot ",
      "{tag_a, tag_b} "
    }),
    .format({
      "slot:%",
      pkg_x::int_to_str($bits(entry_t)),
      "u ",
      "fmt_a:%FA fmt_b:%FB fmt_c:%FC"
    })
  ) u_inst (
    .clk(clk),
    .data(some_signal)
  );

  // Param connection with a wrapped concat too.
  some_module #(
    .INIT_STR({
      "alpha-beta-gamma-delta-epsilon-zeta",
      "eta-theta-iota-kappa-lambda-mu-nu"
    }),
    .N(8)
  ) u2 (.clk(clk));
endmodule
