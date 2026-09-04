// Ports with mixed base-type widths align their shared packed dimensions before
// aligning declarator names.
module port_packed_dimension_align (
    input logic [lane_count-1:0] input_valid,
    input datapath_pkg::word_t [lane_count-1:0] input_data,
    output logic [lane_count-1:0] output_valid,
    output datapath_pkg::word_t [lane_count-1:0] output_data
);
endmodule

// Contrast: a mixed group collapses the packed-dimension subcolumn.
module port_mixed_dimension_align (
    input logic clk,
    input datapath_pkg::word_t [lane_count-1:0] input_data,
    output logic [lane_count-1:0] output_valid
);
endmodule
