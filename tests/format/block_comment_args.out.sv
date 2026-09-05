module test;

    // Block comments as arg names — short args stay inline
    always_comb begin
        result = compute_hash( /* key */input_key, /* seed */ hash_seed, /* rounds */ num_rounds);
    end

    // A long sole-RHS call moves one indent past the assignment line due to
    // width, and its block-comment arguments indent beneath it.
    always_comb begin
        result =
            compute_something(
                /* first_operand */
                very_long_variable_name_alpha,  /* second_operand */
                very_long_variable_name_beta,  /* third_operand */
                very_long_variable_name_gamma
            );
    end

    // Line comments force vertical regardless of width
    always_comb begin
        result =
            compute_hash(
                input_key,  // key
                hash_seed,  // seed
                num_rounds  // rounds
            );
    end

endmodule
