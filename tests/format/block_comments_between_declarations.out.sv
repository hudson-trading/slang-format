// Trailing block comments align consistently after declarations are split onto lines.
module foo;
    reg [0:1] value;  /* comment a */
    reg [0:2] next;   /* comment b */
    reg [0:3] other;
endmodule
