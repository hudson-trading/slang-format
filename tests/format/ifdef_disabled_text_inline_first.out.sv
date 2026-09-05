// `ifdef inside an instance's port-connection list: the disabled branch
// includes a separator comma that sits inline with `\`ifdef SOME_DEFINE`,
// followed by the actual port line on the next row. The baseline scanner
// must look past tokens that have no leading newline (the inline comma);
// otherwise it picks baseline=0 and each reformat pass adds another
// block of indent to the real content line, making formatting non-
// idempotent.

module dut;
    SomeModule
      inst (
        .port_a (port_a),
        .port_b (port_b),
        .port_c (port_c)

    `ifdef SOME_DEFINE,
        .port_d
    `endif
    );
endmodule
