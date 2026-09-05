module top #(
    parameter bit ENABLE = 0
) (
    input wire clk,
    input wire rst
);

    `SETUP_CLK_RST(clk)
    `SETUP_PARAMS(rst)

    generate
        if (ENABLE) begin : gen_block
            assign x = 1;
        end
    endgenerate

endmodule
