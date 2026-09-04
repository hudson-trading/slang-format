// A function invocation that is the entire assignment RHS moves to the next
// line when it must wrap. Its arguments indent from that call line, and named
// arguments align their opening parentheses.
module sole_rhs_invocation_align;
    always_comb begin
        formatted_result =
            example_pkg::build_record(
                .record_kind(example_pkg::RECORD_REPLACE),
                .operation_mode(example_pkg::OPERATION_REUSE),
                .generation_value({2 {example_pkg::generation_t'(request_state.generation)}}),
                .direction_mode(example_pkg::DIRECTION_UNCHANGED),
                .direction(example_pkg::decode_direction(request_state.direction)),
                .update_policy(example_pkg::UPDATE_IN_PLACE)
            );

        // A long expression inside a single argument still puts the call on the
        // next line and gives the argument expression a nested continuation.
        ratio_results[index] =
            example_math_pkg::from_ratio(real'(ratio_table[index].denominator)
                                             / real'(ratio_table[index].numerator));

        // Contrast: a short sole-RHS call stays inline.
        short_result = transform(input_value);
        shifted_a = shift_right(input_a.payload, shift_a);
        shifted_b = shift_right(input_b.payload, shift_b);

        // Contrast: the invocation is not the sole RHS when another expression
        // precedes it, so ordinary expression-continuation indentation applies.
        combined_result = initial_value
                              + example_pkg::build_record(
                                  .record_kind(example_pkg::RECORD_REPLACE),
                                  .operation_mode(example_pkg::OPERATION_REUSE),
                                  .update_policy(example_pkg::UPDATE_IN_PLACE)
                              );
    end

    // Contrast: pending member indentation is not a newline inside either call.
    function automatic logic [15:0] shift_pair(
        input logic [15:0] input_a,
        input logic [15:0] input_b
    );
        logic choose_first;
        logic [15:0] max_value, shift_a, shift_b;
        logic [15:0] xa, xb, shifted_sum;
        choose_first = input_a >= input_b;
        max_value = choose_first ? input_a : input_b;
        shift_a = max_value - input_a;
        shift_b = max_value - input_b;
        xa = shift_right(input_a, shift_a);
        xb = shift_right(input_b, shift_b);
        shifted_sum = xa + xb;
        shift_pair = shifted_sum;
    endfunction
endmodule
