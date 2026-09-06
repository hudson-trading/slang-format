// A wrapped assignment must not pull a declaration across a group boundary.
module separated_declarations;
    wire ready = input_valid & output_ready;

    // This declaration belongs to a separate section.
    wire substantially_longer_condition =
        enabled && (input_channel == saved_channel) && input_valid && input_ready;

    localparam int SHORT_DELAY = 1;

    // This parameter also belongs to a separate section.
    localparam int SUBSTANTIALLY_LONGER_DELAY_CONFIGURATION_WITH_EXTRA_PADDING =
        enabled && input_valid && input_ready ? 2 : 1;
endmodule

module adjacent_declarations;
    wire ready = input_valid & output_ready;
    wire substantially_longer_condition =
        enabled && (input_channel == saved_channel) && input_valid && input_ready;
endmodule
