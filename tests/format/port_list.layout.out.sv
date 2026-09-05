module port_list #(
    parameter int WIDTH = 8,
    parameter int DEPTH = 4,
    localparam int ADDR_W = $clog2(DEPTH)
) (
    input logic clk,
    input logic rst_n,
    input logic [WIDTH-1:0] wr_data,
    input logic [ADDR_W-1:0] wr_addr,
    input logic wr_en,
    output logic [WIDTH-1:0] rd_data,
    input logic [ADDR_W-1:0] rd_addr
);
    logic [WIDTH-1:0] mem[DEPTH];

    always_ff @(posedge clk) begin
        if (wr_en)
            mem[wr_addr] <= wr_data;
        rd_data <= mem[rd_addr];
    end
endmodule
