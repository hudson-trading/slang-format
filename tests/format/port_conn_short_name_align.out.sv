// Regression: short port-connection names (.clk) should still pad to the
// group's max-width column when the group has wider siblings (.refclk_100).
// The per-row alignment speculation must not strip alignment from a row
// whose only "cost" is some extra padding before `(`.

module top;
    inner
      u (
        .clk             (clk_fast),
        .rst             (rst_fast),
        .reference_clock (reference_clock),
        .rx_serial_p     (rx_serial_p)
    );

    // After an `ifdef/endif`, the next port-connection group should still
    // align across the short and long names.
    inner
      v (
    `ifdef SIM
        .sim_transceiver_data_from_device(sim_transceiver_data_from_device),
    `endif
        .clk             (clk_fast),
        .rst             (rst_fast),
        .reference_clock (reference_clock),
        .rx_serial_p     (rx_serial_p)
    );
endmodule
