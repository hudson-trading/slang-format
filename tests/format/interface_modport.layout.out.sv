interface axi_lite_if #(
    parameter int ADDR_WIDTH = 32,
    parameter int DATA_WIDTH = 32
) (
    input logic clk,
    input logic rst_n
);
    logic [ADDR_WIDTH-1:0] awaddr;
    logic awvalid;
    logic awready;
    logic [DATA_WIDTH-1:0] wdata;
    logic wvalid;
    logic wready;
    logic [1:0] bresp;
    logic bvalid;
    logic bready;

    modport master (
        output awaddr, awvalid, wdata, wvalid, bready,
        input awready, wready, bresp, bvalid
    );

    modport slave (
        input awaddr, awvalid, wdata, wvalid, bready,
        output awready, wready, bresp, bvalid
    );
endinterface

module axi_user (
    axi_lite_if.master bus
);
    assign bus.awaddr = 32'h1000;
    assign bus.awvalid = 1'b1;
endmodule
