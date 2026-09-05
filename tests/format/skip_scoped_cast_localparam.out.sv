// Regression: a `skip`ped member must not join an alignment group with its
// neighbors. The skip path emits the member verbatim via printDirective and
// does not advance the per-group row cursor, so a skipped member sharing a
// group with the following member made that member re-emit the skipped row
// (duplicating it and producing a CST mismatch). A scoped (package::) type +
// scoped cast value is what surfaced this in the wild.
module m;
    // slang-format: skip
    localparam foo_pkg::addr_t start_addr_a = foo_pkg::addr_t'(start_addr);
    localparam foo_pkg::addr_t control_end_addr = start_addr_a
                                                  + foo_pkg::addr_t'(depth << $clog2(alignment));

    // Contrast: consecutive un-skipped localparams still align normally.
    localparam int aa   = 1;
    localparam int bbbb = 22;
endmodule
