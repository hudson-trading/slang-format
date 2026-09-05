module select_expressions(
    input logic [7:0] data,
    output logic [3:0] out
);
    logic [31:0] wide;
    logic r;

    // Basic bit and range selects
    assign out = data[3];
    assign out = data  [  7 : 4  ];
    assign out = wide[ 8 +: 4 ];
    assign out = wide[ 11 -: 4 ];

    // Expressions inside selects
    assign r = data[ a + b ];
    assign r = wide[a + b : c - d];

    // Function call in select (args keep spaces)
    assign r = data[f(a, b)];
    assign r = wide[f(x, y) +: g(z)];

    // Nested selects
    assign r = wide[7:0][3:0];

    // Packed dimensions in data types (no space between ][)
    logic [7:0] [3:0] packed_2d;
    logic [15:0]  [7:0]  [3:0] packed_3d;
    logic [7:0]  [3:0] packed_arr [4];

    // inside with value ranges
    always_comb begin
        if (data inside { [0:7], [  16  :  31  ] }) begin
            r = 1;
        end
    end

endmodule
