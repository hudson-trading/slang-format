// Aligned-table format for chained ternaries: conditions are padded so the
// `?` operators line up in one column. The `:` trails each segment's line;
// the final-else value sits on its own line indented to the value column
// (one past the aligned `?`). Internal wraps inside a value continue at the
// value column rather than the chain's block indent.
//
// Pattern:
//     lhs =
//         cond_a       ? value_a :
//         cond_longer  ? value_b :
//                        value_default;

module test;
    // Varying condition widths -- alignment matters.
    always_comb begin
        result_values[i] =
            sample_values[i] < limits[0].ranges[i].minimum ? result_t'(limits[0].ranges[i].minimum) :
            sample_values[i] > limits[0].ranges[i].maximum ? result_t'(limits[0].ranges[i].maximum) :
                                                             result_t'(sample_values[i]);
    end

    // Three-way chain with mixed condition widths.
    assign sel = (mode == FAST) ? 2'b00 : (mode_extended_flag == SLOW) ? 2'b01 : 2'b11;

    // Fallback: conditions so wide that aligning the `?` column would push
    // it past the column limit. Chain stays flat with `:` trailing each
    // line, but no padding between condition and `?`. The final-else value
    // is still padded to align with the segment values, even though the
    // resulting column sits past the column limit (matching where those
    // values land when conditions are uniform width).
    always_comb begin
        bus_sel =
            (this_is_a_very_long_condition_a_a_a_a_a_a_a == EXTRA_LONG_SYMBOL_THAT_DOESNT_FIT_INLINE) ? 8'h01 :
            (this_is_a_very_long_condition_b_b_b_b_b_b_b == EXTRA_LONG_SYMBOL_THAT_DOESNT_FIT_INLINE) ? 8'h02 :
                                                                                                        8'hFF;
    end

    // Value-internal wrap aligns to the value column, not block indent.
    always_comb begin
        pipeline_output.data.remaining =
            pipeline_input.data.remaining == '0 ? '0 :
            !is_last_item                       ? (amount_per_item - adjustment) :
            is_first_item                       ? (pipeline_input.data.remaining - adjustment) :
                                                  (amount_remaining - adjustment);
    end

    // A long single ternary uses a consistent branch group: either it stays
    // inline or both `?` and `:` start expression-relative continuation lines.
    always_comb begin
        metric_is_within_limit <=
            direction
                ? (new_sample.metric <= accumulated.average)
                : (new_sample.metric >= accumulated.average);
    end
endmodule
