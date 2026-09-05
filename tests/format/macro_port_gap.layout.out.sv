module test (
    input wire clk,
    input wire rst,
    input wire clken,

    `MY_PORT(size64_t, data),
    `MY_PORT(logic, valid)
);

endmodule
