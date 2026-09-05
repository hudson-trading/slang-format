module property_sequence (
    input logic clk,
    input logic rst_n,
    input logic a, b, c
);

    // Property with local variables
    property p_req_ack;
        logic v;
        @(posedge clk) disable iff(!rst_n) a |-> ## [1:3] b;
    endproperty

    // Sequence with local variables
    sequence s_handshake;
        logic tmp;
        a ## 1 b ## 1 c;
    endsequence

    // Property without local variables
    property p_simple;
        @(posedge clk) a |-> b;
    endproperty

    assert property (p_req_ack);
    assert property (p_simple);

endmodule
