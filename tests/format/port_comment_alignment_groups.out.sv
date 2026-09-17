// Comments align within a port group after wide declarations force regrouping.
module demo (
    output logic                                         enabled,
    output example_types_pkg::extended_metadata_record_t metadata,
    output logic                                         request_start,
    input  wire  request_ready,  // Pulses when the consumer starts accepting the next record.
    input  wire  done,           // Marks the end of a record.
    input  wire  item_done,      // Marks the end of an item within the current record.
    output logic fast_mode,      // Selects accelerated handling.
    output sample_pkg::origin_t origin,  // Source selection.
    output sample_pkg::record_t record,
    output sample_pkg::length_t length
);
endmodule
