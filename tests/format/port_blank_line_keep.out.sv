// Port groups merge across a single blank line (port-list threshold is 2
// separator lines). A standalone comment region contributes N-1 of its physical
// lines, so a one-line annotation is free while a longer section header can
// help split the group.

module dut (
    input logic   clk,
    input logic   rst,

    PortA.source  tx_data,
    PortA.monitor rx_data,
    PortB.slave   control,

    // These three comment lines contribute two separators, so they split the
    // group even without the single blank above. Input/interface ports above
    // and output ports below therefore use separate alignment tables.
    output logic             inc_count_a,
    output dut_pkg::credit_t new_count_a,

    output logic             inc_count_b,
    output dut_pkg::credit_t new_count_b
);
endmodule
