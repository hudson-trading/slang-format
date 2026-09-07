// Formatting a multiline macro invocation inside an inactive conditional
// branch must not add indentation on each pass.
module inactive_conditional_multiline_macro_idempotent;
`ifdef OPTIONAL_CHECKS
    `CHECK_NEVER(check_name,
        first_condition && second_condition)

    `CHECK_IMPLICATION(implication_name,
        first_condition && second_condition,
        result_condition
    )
`endif
endmodule
