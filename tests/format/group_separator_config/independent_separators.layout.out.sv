// List groups split on one blank line, while statements and declarations
// require two with this directory's config. Adjacent rows still align.
module foo (
    input logic a,
    input logic [7:0] b,

    input logic c,
    input logic d
);
    logic [7:0] first;
    logic second;

    logic third;
    logic fourth;


    logic fifth;
    logic sixth;

    typedef struct packed {
        logic [7:0] a;
        logic b;

        logic c;
        logic d;
    } data_t;

    always_comb begin
        long_name = 1;
        a = 2;

        b = 3;
        cc = 4;


        d = 5;
        ee = 6;
    end
endmodule
