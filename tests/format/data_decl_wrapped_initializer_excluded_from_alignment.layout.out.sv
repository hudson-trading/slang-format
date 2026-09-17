// A declaration whose initializer wraps after `=` does not participate in
// type/name alignment with adjacent uninitialized declarations.
module wrapped_initializer;
    logic short_flag;
    processing_metadata_pkg::available_count_t count_distance;
    wire packet_count_care = packet_check.valid & packet_check.start
                             & packet_meta.start.free_status[index].valid;
endmodule

module inline_initializer;
    // Contrast: an initialized declaration that stays on one line still aligns.
    logic short_flag;
    processing_metadata_pkg::available_count_t count_distance;
    wire packet_count_care = packet_check.valid;
endmodule
