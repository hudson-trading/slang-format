module assertions (
    input logic clk,
    input logic rst_n,
    input logic req,
    input logic ack,
    input logic valid,
    input logic ready
);
    // Immediate assertions
    always_comb begin
        assert (req || !ack) else $error("ack without req");
        assume (valid -> ready) else $warning("valid without ready");
    end

    // Concurrent assertions
    property req_ack_p;
        @(posedge clk) disable iff(!rst_n) req |-> ## [1:3] ack;
    endproperty

    property handshake_p;
        @(posedge clk) disable iff(!rst_n) valid && ready |=> !valid;
    endproperty

    assert_req_ack: assert property (req_ack_p) else $error("req not followed by ack");

    assume_handshake: assume property (handshake_p);

    // Sequences
    sequence burst_seq;
        @(posedge clk) valid ## 1 valid[*3] ## 1 !valid;
    endsequence

    cover_burst: cover property (burst_seq);

    // Cover property
    cover property (@(posedge clk) req ## 1 ack);
endmodule
