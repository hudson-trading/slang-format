module child #(
    parameter int WIDTH = 8
) (
    input logic [WIDTH-1:0] in,
    output logic [WIDTH-1:0] out
);
    assign out = ~in;
endmodule

module parent (
    input logic clk,
    input SomePkg::some_t [7:0] a,
    output logic [7:0] b,
    output logic [15:0] c
);
    // Named port connection
    child #(.WIDTH(8))
      u_byte (.in(a), .out(b));

    // Wider instance
    child #(.WIDTH(16))
      u_half (.in({a, a}), .out(c));

    // Longer name, verticalize
    somemodule #(
        .WIDTH(16)
    ) u_half (
        .asdf({a, a}),
        .fda(c)
    );
endmodule
