// Recovered continuation comments must not change expression indentation.
assign result_bit = mode_value == 2'b00 ? fraction_bits[2]
                    & ((fraction_bits[1] | sticky) | fraction_bits[3]) :
                    mode_value == 2'b11 ?  // round down
                    (fraction_bits[2] | fraction_bits[1] | sticky)
                    & sign_bit :  // round to zero (truncate without rounding)
                                          0;

// A comment can attach to the preceding colon after formatting.
assign output_data = first_request ? {
                         15'b0,
                         final_count,
                         source_id,
                         command_data
                     } :  // padding, count, source, command
                     next_request  ? {15'b0, final_count, source_id, input_data[11:0]} :
                                     '0;
