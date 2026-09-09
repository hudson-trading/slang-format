// Single-line block comments at declaration ends align, including before semicolons.
module foo;
    logic ab /*verilator public*/;
    logic longer /*verilator public*/;
    data_t value /*verilator public*/;

    logic ab;  /* first */
    logic middle;  /* second */
    data_t other;  /* third */

    logic ab;  /* block */
    logic longer;  // line
    logic value /* before semicolon */;

    // Wide gaps still split comment alignment groups.
    logic very_long_name /* first */;
    logic ab /* second */;
    logic xyz /* third */;

    // Comments inside declarations retain their local spacing.
    logic /* type */ x;
    logic /* type */ longer_name;

    // Multiline comments keep their internal indentation.
    logic a /* first
    continued */;
    logic longer /* second
    continued */;
endmodule
