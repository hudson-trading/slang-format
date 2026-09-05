module configurable_unit #(
    parameter int INPUT_WIDTH  = 8,
    parameter int INPUT_SCALE  = 4,
    parameter int AUX_WIDTH    = 16,
    parameter int AUX_SCALE    = 8,
    parameter int RESULT_WIDTH = 16,
    parameter int RESULT_SCALE = 8
) (
    input logic clock
);
endmodule

module top;
    configurable_unit #(
        .RESULT_WIDTH (16),
        `PARAM_GROUP(first_cfg, first_cfg),
        .RESULT_SCALE (8)
    ) u_config (.clock(clock));

    ProcessingUnit #(
        `PARAM_GROUP(first, intermediate_cfg),
        `PARAM_GROUP(second, factor_cfg),
        `PARAM_GROUP(result, result_cfg),
        .round_mode ('0),
        .high_limit (make_limit(+1.0)),
        .low_limit  (make_limit(-1.0))
    ) u_processing (
        .clock       (clock),
        .first_data  (input_value),
        .second_data (factor_value),
        .offset      ('0),
        .result_data (result_value)
    );
endmodule
