`include "macro_assign_type_param_recovery_defs.svh"

module macro_assign_type_param_recovery #(
    parameter int  block_beats,
    parameter type data_t,
    parameter type pkt_size_t,
    parameter int  num_descriptors,
    parameter type desc_t
) (
    InputStream.sink    input_stream,
    OutputStream.source output_stream
);
    `DECLARE_INTERFACE_CLOCK(input_stream)
    `DECLARE_INTERFACE_PARAMS(output_stream)

    localparam int beat_count_width = $clog2(MAX_ITEMS / output_items_per_transfer);
    typedef logic [beat_count_width-1:0] beat_count_t;

    localparam int data_count_width = $clog2(block_beats);
    typedef logic [data_count_width-1:0] data_count_t;

    `ASSIGN_COMB(desc_t, descriptor) = desc_t'(input_stream.data);

    data_count_t  output_block_count;
    beat_count_t  output_packet_count;
    empty_count_t output_empty_inverse;

    pkt_size_t [num_descriptors-2:0] output_packet_sizes;
endmodule
