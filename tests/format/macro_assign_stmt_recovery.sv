`include "test_defs.svh"

module top;
    `DECLARE_INTERFACE_CLOCK(input_stream)
    `DECLARE_INTERFACE_PARAMS(output_stream)

    logic clk;
    logic pause_input;
    count_t buffered_transfer_count;
    count_t maximum_buffered_transfer_count;
    count_t required_transfer_count;
    typedef logic descriptor_t;

    // Issue a pause when reaching the halfway mark
    always_ff @(posedge clk) pause_input <=
        buffered_transfer_count >
        count_t'(maximum_buffered_transfer_count);

    // Extract only the event header before serializing the input.
    `ASSIGN_COMB(descriptor_t, descriptor) = descriptor_t'(input_stream.data);
    `DELAY_VALUE(descriptor_t, descriptor, 0, 1)
    `ASSIGN_COMB(descriptor_t, restored_descriptor) = restoreDescriptor(descriptor);
    data_count_t output_block_count;
    beat_count_t output_packet_count;
    empty_count_t output_empty_inverse;

    `ASSIGN_COMB(protocol_pkg::serialized_event_header_t, incoming_serialized_event_header) = protocol_pkg::serialized_event_header_t'(incoming_serialized_event.data);

    `ASSIGN_COMB(count_t, required_transfer_count) = protocol_pkg::count_t'(incoming_serialized_event_header.size >> $clog2(symbols_per_transfer));

    // Store-and-forward gate: wait until whole event is in the FIFO (size from header)
    `ASSIGN_COMB(logic, incoming_event_fully_buffered) = incoming_serialized_event.valid && buffered_transfer_count >= transfer_buffer_level_t'(required_transfer_count);

    count_t required_transfer_count_minus_one;
endmodule
