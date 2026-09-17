// When breaking after `=` still leaves an overlong line, expression-only
// breaks compete with it, keeping the RHS inline when line counts tie.

package test_pkg;
    localparam int decode_latency = decode_pipeline_input_register_stage
                                    + decode_pipeline_rounding_register_stage
                                    + decode_pipeline_output_register_stage;

    // Contrast: when the RHS fits after `=`, prefer the single outer break.
    localparam int lookup_latency =
        compute_lookup_latency(first_lookup_stage, second_lookup_stage);
endpackage
