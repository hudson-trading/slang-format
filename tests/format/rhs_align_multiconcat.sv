// When the RHS of `=` is a multi-concat `{N {...}}` whose inner list
// verticalizes, the items align to the column after `=` rather than
// snapping back to the surrounding block indent. The closing `}};`
// lands at the alignment column too.
//
// Top-level concats and assignment patterns keep block indent because
// the verticalized items would otherwise crowd far to the right of
// their opening token with no syntactic benefit.

module rhs_align_multiconcat;
    parameter int32_t [templates_num_fields-1:0] templates_hw_sizes = {num_instruction_templates {a, b, c, d}};

    parameter int32_t [n-1:0] long_items = {n_copies {long_item_one_zero_zero, long_item_two_zero_zero, long_item_three_zero_zero, long_item_four_zero_zero}};

    // Aligned items that would overflow the column limit fall back to
    // block indent so the right margin stays respected.
    parameter int32_t [some_long_count_constant_name-1:0] overflow_items = {n_copies_long_name_here {long_item_one_with_extra_chars, long_item_two_with_extra_chars, long_item_three_with_extra}};

    // Top-level concat (not multi-concat) uses block indent regardless.
    parameter int32_t [n-1:0] flat_concat = {item_one_long_long, item_two_long_long, item_three_long_long, item_four_long_long};

    // Assignment-pattern (`'{...}`) uses block indent regardless.
    parameter cfg_t cfg = '{alpha: 1, beta: 2, gamma: 3};
endmodule
