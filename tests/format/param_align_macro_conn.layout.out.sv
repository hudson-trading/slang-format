// A bare macro standing in for an instance's port connections
// (`u_inst (`MACRO_CONNECT)`) must NOT force the whole instantiation through
// verbatim emission — that would discard the parameter-list alignment. The
// `#(...)` parameters here should align their `(` columns across the blank
// lines just as they would with ordinary `.port(net)` connections.
module top
    VUPIO #(
        .board_type(board_type),

        .primary_phy(0),
        .primary_25g_phy(0),
        .primary_slr(primary_slr),

        .port_used(16'h000F),
        .port_has_config_endpoint(16'h0001)
    ) vupio (
        `BOILERPLATE_CONNECT
    );

    // Contrast: ordinary connection — same alignment.
    OTHER #(
        .width(8),
        .depth_in_words(16)
    ) u_other (.clk(clk));
endmodule
