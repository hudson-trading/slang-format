// A long cast in a named connection breaks after the cast's opening parenthesis,
// giving its expression a useful anchor instead of splitting a short expression.
module cast_expression_break_after_open_paren;
    some_module #(
        .width(8)
    ) u_long (
        .clk(clk),
        .result_index(architecture_pkg::long_result_index_t'(
                          architecture_pkg::long_pipeline_latency - 1))
    );

    // Contrast: a cast that fits stays inline.
    some_module #(
        .width(8)
    ) u_short (
        .clk(clk),
        .result_index(architecture_pkg::result_index_t'(pipeline_latency - 1))
    );
endmodule
