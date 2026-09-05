module clocking_test (
    input logic clk,
    input logic rst_n
);
    logic [7:0] data,  addr;
    logic       valid, ready;

    clocking cb @(posedge clk);
        default input #1step output #0;
        input valid;
        input ready;
        output data;
        output addr;
    endclocking

    clocking monitor_cb @(posedge clk);
        default input #1ns;
        input data, addr, valid, ready;
    endclocking

    default clocking cb;
endmodule
