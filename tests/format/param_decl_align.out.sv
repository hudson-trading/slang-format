// Parameter/localparam declarations align across rows: the `=` lines up
// regardless of name widths. Trailing `;` sits flush against the value
// (no padding between value and semi).

module top;
    localparam int num_builder     = some_pkg::struct_builder_num_builder;
    localparam int num_bram        = some_pkg::struct_builder_num_bram;
    localparam int max_struct_size = some_pkg::proc_event_max_bytes;
    localparam int token_size      = some_pkg::token_bytes;

    parameter int token_offset_width = $clog2(max_struct_size);
    parameter int token_size_width   = $clog2(token_size) + 1;

    // Mixed localparam/parameter rows align both the keyword/type boundary
    // and the declaration names.
    localparam int a = 1;
    parameter  int b = 2;

    // Implicit-type parameters (no data type) leave the type column empty —
    // the name sits one space after the keyword, not two.
    parameter num_cond_operand_A = 6;
    parameter num_cond_operand_B = 7;
    parameter cond_operand_width = 64;

    // Mixed explicit/implicit types in one group: the implicit row's name
    // still lines up with the explicitly-typed rows' names.
    parameter int         WIDTH    = 6;
    parameter             num_cond = 7;
    parameter logic [3:0] MASK     = 64;

    // Wrapped rows do not retain padding whose only purpose was to align an
    // equals sign with another declaration in the group.
    localparam int chunk_count =
        utility_pkg::ceil_divide(payload_size_in_bytes, output_bytes_per_cycle);
    localparam int payload_transport_response_num_padding_bytes =
        chunk_count * output_bytes_per_cycle - int'(payload_size_in_bytes);
endmodule
