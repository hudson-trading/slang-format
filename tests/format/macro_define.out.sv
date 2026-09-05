`define WIDTH 8
`define DEPTH 256
`define ADDR_W $clog2(`DEPTH)
`define MAX(a, b) ((a) > (b) ? (a) : (b))
`define ASSERT_RST(sig) \
    assert property (@(posedge clk) $rose(rst_n) |-> !sig)

module macro_test (
    input  logic              clk,
    input  logic              rst_n,
    input  logic [`WIDTH-1:0] data_in,
    output logic [`WIDTH-1:0] data_out
);
    logic [`ADDR_W-1:0] addr;
    logic [`WIDTH-1:0]  mem [`DEPTH];

    always_ff @(posedge clk) begin
        mem[addr] <= data_in;
        data_out  <= mem[addr];
    end

    localparam int BIGGEST = `MAX(10, 20);
endmodule
