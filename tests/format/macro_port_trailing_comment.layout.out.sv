// A comment after a port-prefix macro stays after that macro.
module foo (
    `UNUSED // Only used in one configuration.
    input logic clk,
    `UNUSED /* Optional input. */
    input logic reset
);
endmodule
