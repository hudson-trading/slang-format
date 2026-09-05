module top (
    input  wire a,
    output wire b
);
    foo #(
        .width (1)
    ) inst (
        .data (a),
        .dout (b),
        `EXTRA_PORTS
    );
endmodule
