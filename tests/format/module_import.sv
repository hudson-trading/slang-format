// Test: import in module header should be on its own line

module simple_mod import pkg::*; #(
    parameter int WIDTH
) (
    input logic clk,
    input logic rst
);
endmodule

module multi_import import pkg_a::*; import pkg_b::*; #(
    parameter int DEPTH
) (
    input logic data
);
endmodule

module import_no_params import some_pkg::*; (
    input logic clk
);
endmodule
