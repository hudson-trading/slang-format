// A macro case label preserves the assignment boundary when a long RHS wraps.
module demo;
    always_comb begin
        case (select_value)
            `ifdef FEATURE_A
                `CASE_LONG_VALUE: result_value = {some_value, {(`SOME_LONG_WIDTH-`OTHER_LONG_WIDTH){1'b0}}};
                `CASE_OTHER_VALUE: alternate_result_value = {some_value, `PACK(first_arg,  second_arg,  third_arg)};
            `endif
            default: result_value = 0;
        endcase
    end
endmodule
