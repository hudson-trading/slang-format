module multiline_expr (
    input  logic [31:0] a, b, c, d, e, f,
    input  logic        sel,
    output logic [31:0] result
);
    // Ternary expression
    assign result = sel ? (a + b + c) : (d + e + f);

    // Concatenation and replication
    logic [63:0] wide;
    assign wide = {a, b};

    logic [255:0] replicated;
    assign replicated = {8 {a}};

    // Complex expression
    logic [31:0] computed;
    always_comb begin
        computed = ((a & b) | (c & d)) ^ ((e & f) | (a & c));
    end
endmodule
