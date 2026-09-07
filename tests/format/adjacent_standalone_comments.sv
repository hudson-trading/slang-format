// Adjacent comments retain source order after declarations and definitions.
module foo;
logic a;
/* lint off */ // Reason for disabling lint.
logic b;
`define ACTION(x) \
    if (x) /* consume */ /* marker */

logic c;
endmodule
