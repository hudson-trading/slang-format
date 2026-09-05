module cast_expressions (
    input  logic [7:0]  a,
    output logic [31:0] b
);
    // Type casts
    assign b = int'(a);
    assign b = int'(a);
    assign b = 32'(a);
    assign b = logic [31:0]'(a);

    // Signed/unsigned casts
    assign b = signed'(a);
    assign b = unsigned'(a);

    // Cast in expression
    assign b = int'(a) + 'd1;
    assign b = int'(a) + unsigned'(a);

    // Void cast
    initial void'($value$plusargs("print_topology=%0b", a));
    initial void'($value$plusargs("print_topology=%0b", a));
endmodule
