// Width conflicts split contiguous port groups so neighboring short ports still align.
module demo (
    input logic enable, // Enables forwarding from the primary source to the selected destination.
    output wire ready,
    input example_types_pkg::extended_metadata_record_t metadata_with_a_long_descriptive_name,
    input wire request_pending, // A request remains pending until its destination accepts it.
    output logic request_accepted,
    input wire response_pending,
    output logic response_accepted,
    sample_if.sink monitor
);
endmodule
