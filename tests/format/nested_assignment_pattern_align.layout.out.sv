// Nested assignment-pattern fields use the inner pattern's indentation and
// alignment; the outer field's alignment must not push inner rows far right.
module nested_assignment_pattern_align;
    always_comb begin
        result <= '{
            valid: enabled,
            payload: '{
                first_value: choose_new_value ? incoming_payload.first_value
                                : saved_payload.first_value,
                count: choose_new_value ? '0 : saved_payload.count
            },
            tag: selected_tag
        };
    end
endmodule
