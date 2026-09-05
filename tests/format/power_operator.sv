// The `**` power operator gets no space on either side, unlike other
// binary operators (`+`, `*`, etc.) which keep their surrounding spaces.
module test;
    localparam int A = 2 ** 8;
    localparam int B = WIDTH**2;
    localparam int C = (x + 1) ** (y - 1);
    localparam int D = a + b ** c * d;
endmodule
