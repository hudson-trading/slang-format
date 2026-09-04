// A macro-provided instance type must choose the same connection layout after
// the formatted text is reparsed.
module example;
    `CLOCK_GATE_TYPE clock_gate (
        .*,
        .enable (enable),
        .clock  (clock),
        .gated  (gated_clock)
    );
endmodule
