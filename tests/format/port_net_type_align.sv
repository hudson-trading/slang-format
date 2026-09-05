// `input wire foo` parses as NetPortHeader (not VariablePortHeader): direction
// + net type + implicit data type. It still participates in port-list
// alignment alongside variable ports (`input logic`), interface ports
// (`MyIfc.source`), etc. — net type sits where the data type would for a
// variable port, so columns line up cleanly across all three kinds.

module dut (
    MyIfc.source        tx_data,
    input pkg_x::time_t ev_time,
    input wire          wd_tripped,
    MyIfc.source        tx_state
);
endmodule
