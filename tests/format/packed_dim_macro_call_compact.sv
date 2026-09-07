// A function-like macro used as an operand does not disable compact spacing
// for the surrounding packed dimension.
module packed_dim_macro_call_compact #(
    parameter int item_count = 8,
    localparam type index_t = logic [`MAX_WIDTH($clog2(item_count), 1) - 1 : 0],

    // Contrast: ordinary packed dimensions use the same compact spacing.
    localparam type plain_t = logic [$clog2(item_count + 1) - 1 : 0]
);
endmodule
