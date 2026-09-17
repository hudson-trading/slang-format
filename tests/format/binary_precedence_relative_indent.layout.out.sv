// Binary continuation indentation reflects precedence groups within a longer expression.
module binary_precedence_relative_indent;
    always_comb result = address_pkg::scalar_memory_address_t'(
        region_selector[0] * arch_pkg::branch_count * address_pkg::folds_per_branch
        + branch_ids[region_selector] * address_pkg::folds_per_branch + control.scalar_index);

    // A flat same-precedence chain keeps one continuation anchor.
    always_comb flat_result = first_long_operand + second_long_operand
                              + third_long_operand + fourth_long_operand;

    // Contrast: a short mixed-precedence expression remains inline.
    always_comb short_result = left * scale + right;
endmodule
