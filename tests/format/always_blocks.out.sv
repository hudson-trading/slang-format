module always_blocks (
    input  logic       clk,
    input  logic       rst_n,
    input  logic [3:0] sel,
    input  logic [7:0] a, b, c, d,
    output logic [7:0] out_ff,
    output logic [7:0] out_comb,
    output logic [7:0] out_latch
);
    // always_ff
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n)
            out_ff <= '0;
        else
            out_ff <= a + b;
    end

    // always_comb with case
    always_comb begin
        case (sel)
            4'd0:    out_comb = a;
            4'd1:    out_comb = b;
            4'd2:    out_comb = c;
            4'd3:    out_comb = d;
            default: out_comb = '0;
        endcase
    end

    // always_latch
    always_latch begin
        if (sel[0])
            out_latch = a;
    end

    // always @*
    logic [7:0] misc;
    always @* begin
        misc = a ^ b;
    end
    // always_ff with if/else (no begin/end wrapper) should indent body
    logic merged_unit_scalar_rd_valid;
    logic scalar_mon_rd_pending;
    logic next_merged_unit_scalar_rd_valid;
    logic next_scalar_mon_rd_pending;

    always_ff @(posedge clk)
        if (rst_n) begin
            merged_unit_scalar_rd_valid <= 1'b0;
            scalar_mon_rd_pending       <= 1'b0;
        end else begin
            merged_unit_scalar_rd_valid <= next_merged_unit_scalar_rd_valid;
            scalar_mon_rd_pending       <= next_scalar_mon_rd_pending;
        end

    // always_comb with single assignment stays inline
    always_comb misc = a;

    // Empty begin/end blocks collapse to one line as a "nothing block"
    // (cf. case_empty_default_block.sv).
    initial begin end
    always_ff @(posedge clk) begin end
    always_comb begin
        if (a) begin end else begin end
    end
endmodule
