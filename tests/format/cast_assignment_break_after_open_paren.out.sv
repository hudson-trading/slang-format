// A long cast keeps its type and opening parenthesis on the assignment line
// when they fit, then indents the operand like a function-call argument.
module cast_assignment_operand_indent;
    always_comb begin
        output_address <= address_pkg::memory_address_t'(
            branch_id * address_pkg::maximum_folds + control_index
            + address_offset + additional_address_offset);
    end
endmodule

package cast_assignment_break_after_open_paren;
    // Contrast: a declaration whose cast header does not fit still breaks at `=`.
    localparam config_pkg::flag_mask_t excluded_flags_with_a_long_declaration_name =
        config_pkg::flag_mask_t'(
            1 << config_pkg::FLAG_ALPHA | 1 << config_pkg::FLAG_BETA | 1 << config_pkg::FLAG_GAMMA
            | 1 << config_pkg::FLAG_DELTA | 1 << config_pkg::FLAG_EPSILON);

    // Contrast: a cast that fits stays inline.
    localparam config_pkg::flag_mask_t included_flags =
        config_pkg::flag_mask_t'(1 << config_pkg::FLAG_ALPHA);
endpackage
