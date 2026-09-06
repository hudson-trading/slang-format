// A blank line after a standalone port-list macro must remain a section boundary.
module foo (
    // Extension ports
    `EXTRA_PORTS(foo)

    // Control ports
    input logic signal_a,
    output logic signal_b
);
endmodule
