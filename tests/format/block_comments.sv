/* File-level block comment */
module block_comments (
    input logic clk,
    output logic out
);
    /* Variable declarations */
    logic a, b;
    logic /* inline comment */ c;

    /*
     * Multi-line block comment
     * describing the always block
     */
    always_comb begin
        a = b /* mid-expression comment */ & c;
        out = a;
    end

    /* trailing block comment */
endmodule
