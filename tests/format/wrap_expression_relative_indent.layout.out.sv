// Binary continuations hang one indent past the expression start, not the member block.
module wrap_expression_relative_indent;
    always_comb memory_address =
        address_pkg::address_t'(address_pkg::address_t'(entry_index)
                                    * address_pkg::address_t'(control_state.fold_count)
                                + address_pkg::address_t'(control_state.memory_offset));

    // A root binary RHS uses the same expression-relative continuation anchor.
    always_comb request_state.ready = deeply_nested_gated_request_state.ready
                                          || (request_state.valid && (disabled || filtered));

    // The nested expression keeps its own anchor when the assignment first breaks at `=`.
    always_comb begin
        resolved_control_entries[index].resolved_memory_address =
            address_pkg::address_t'(branch_index * address_pkg::memory_depth
                                    + controls[index].memory_address);
    end

    // Contrast: a short expression remains inline.
    always_comb short_address = address_pkg::address_t'(entry_index + 1);
endmodule
