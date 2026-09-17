// Edge case tests for expression line wrapping (Formatter_wrap.cpp)

module line_wrapping;
    logic [31:0] alpha_long_signal, beta_long_signal, gamma_long_signal, delta_long_signal;
    logic [31:0] epsilon_long_signal, zeta_long_signal;
    logic [31:0] result, output_val;
    logic        condition_long_flag, enable_long_flag, ready_long_flag;

    // ---- Edge case 1: Parenthesized expressions ----
    // Parens stay intact; inner expression wraps independently
    assign result = (alpha_long_signal + beta_long_signal + gamma_long_signal
                         + delta_long_signal + epsilon_long_signal);

    // ---- Edge case 2: Concatenation ----
    // Handled by Dynamic list verticalization, NOT expression wrapping
    logic [159:0] wide_concat;
    assign wide_concat = {
        alpha_long_signal,
        beta_long_signal,
        gamma_long_signal,
        delta_long_signal,
        epsilon_long_signal
    };

    // ---- Edge case 3: Function calls with long arguments ----
    // A long sole-RHS call moves to the next indented line; individual args
    // may wrap beneath it.
    function automatic logic [31:0] compute(input logic [31:0] x, input logic [31:0] y);
        return x + y;
    endfunction
    assign result =
        compute(
            alpha_long_signal + beta_long_signal + gamma_long_signal,
            delta_long_signal + epsilon_long_signal
        );

    // ---- Edge case 4: Nested ternary ----
    assign result =
        condition_long_flag
            ? enable_long_flag
                ? alpha_long_signal + beta_long_signal
                : gamma_long_signal + delta_long_signal
            : epsilon_long_signal + zeta_long_signal;

    // ---- Edge case 5: Mixed precedence ----
    // Lower precedence (+) breaks before higher precedence (*)
    assign result = alpha_long_signal + beta_long_signal * gamma_long_signal
                    + delta_long_signal * epsilon_long_signal;

    // Mixed logical and arithmetic: || breaks before &&
    assign output_val =
        (alpha_long_signal > beta_long_signal) && (gamma_long_signal < delta_long_signal)
        || (epsilon_long_signal != '0);

    // ---- Binary chain wrapping (same precedence) ----
    // Long chain of && operators — rightmost break that fits
    assign output_val = condition_long_flag && enable_long_flag && ready_long_flag
                        && (alpha_long_signal > beta_long_signal)
                        && (gamma_long_signal < delta_long_signal);

    // Long chain of + operators
    assign result = alpha_long_signal + beta_long_signal + gamma_long_signal
                    + delta_long_signal + epsilon_long_signal;

    // Long chain of + operators but user split across lines
    assign result = alpha_long_signal + beta_long_signal + gamma_long_signal
                    + delta_long_signal + epsilon_long_signal;

    // User break inside higher-precedence subexpression — formatter should
    // prefer the lower-precedence (+) break over the user's (*) break
    assign result = alpha_long_signal + beta_long_signal * gamma_long_signal
                    + delta_long_signal + epsilon_long_signal;

    // User break early in chain — formatter respects it instead of rightmost-that-fits
    assign result = alpha_long_signal + beta_long_signal + gamma_long_signal
                    + delta_long_signal + epsilon_long_signal;

    // User break in && chain — respected at same precedence
    assign output_val = condition_long_flag && enable_long_flag && ready_long_flag
                        && (alpha_long_signal > beta_long_signal);

    // User break matches where formatter would break — same output
    assign result = alpha_long_signal + beta_long_signal + gamma_long_signal
                    + delta_long_signal + epsilon_long_signal;

    // No user break — formatter uses rightmost-that-fits
    assign result = alpha_long_signal + beta_long_signal + gamma_long_signal
                    + delta_long_signal + epsilon_long_signal;

    // ---- Assignment wrapping ----
    // Non-blocking assignment wraps after <=
    always_ff @(posedge condition_long_flag) begin
        result <= alpha_long_signal + beta_long_signal + gamma_long_signal
                  + delta_long_signal + epsilon_long_signal;
    end

    // Blocking assignment with ternary RHS
    always_comb begin
        result =
            condition_long_flag ? alpha_long_signal + beta_long_signal
                                     + gamma_long_signal : delta_long_signal + epsilon_long_signal;
    end

    // ---- Assignment with concatenation RHS (should NOT wrap at =) ----
    logic [159:0] concat_result;
    assign concat_result = {
        alpha_long_signal,
        beta_long_signal,
        gamma_long_signal,
        delta_long_signal,
        epsilon_long_signal
    };

    // ---- Triple nested ternary (innermost fits inline) ----
    assign result =
        condition_long_flag
            ? enable_long_flag
                ? ready_long_flag
                    ? alpha_long_signal
                    : beta_long_signal
                : gamma_long_signal
            : delta_long_signal;

    // ---- Triple nested ternary (all three wrap) ----
    assign result =
        first_condition_variable
            ? second_condition_variable
                ? third_condition_variable
                    ? alpha_long_signal + beta_long_signal
                    : gamma_long_signal + delta_long_signal
                : epsilon_long_signal + zeta_long_signal
            : alpha_long_signal + epsilon_long_signal;

    // ---- Nested expression-relative indentation ----
    // Each wrapped operator chain continues one indent past its expression start.
    logic [7:0]  status_flags;
    logic [31:0] mode_select, active_bitmap;
    logic [31:0] field_a, field_b, field_c;
    logic        info_valid;
    always_comb begin
        status_flags[0] = ((mode_select == 32'h01 || mode_select == 32'h02)
                               && info_valid && ((active_bitmap & (1 << field_a)) == 0))
                          || (mode_select == 32'h03 && ((active_bitmap & (1 << field_b)) == 0))
                          || (mode_select == 32'h04 && ((active_bitmap & (1 << field_c)) == 0));
    end

    // ---- Outermost chain no-bump: simple && that stays flat ----
    logic combined_valid;
    logic upstream_ready_long_name, downstream_ack_long_name;
    always_comb begin
        combined_valid = upstream_ready_long_name && downstream_ack_long_name;
    end

    // ---- Deep nesting: && with repeated || pattern inside parens ----
    // Tests multi-level: assignment -> && -> || -> inner && -> ==
    logic        valid_result;
    logic [31:0] stage_valid, category_field_alpha, category_field_beta, category_field_gamma;
    logic [31:0] check_result_alpha, check_result_beta, check_result_gamma;
    parameter int num_stages = 4;
    always_ff @(posedge clk) begin
        valid_result <=
            stage_valid
            && (((category_field_alpha == alpha_long_signal) && (check_result_alpha != '0))
                    || ((category_field_beta == beta_long_signal) && (check_result_beta != '0))
                    || ((category_field_gamma == gamma_long_signal) && (check_result_gamma != '0)));
    end

    // ---- Nothing fits: first operand already exceeds column limit ----
    // When no break point keeps the left side under the limit, the formatter
    // picks the lowest-precedence operator anyway to make progress.
    logic [31:0] extremely_long_variable_name_that_exceeds_the_column_limit_on_its_own;
    assign result = extremely_long_variable_name_that_exceeds_the_column_limit_on_its_own
                    + alpha_long_signal;

    // Nothing fits with nested precedence
    assign result =
        extremely_long_variable_name_that_exceeds_the_column_limit_on_its_own * alpha_long_signal
        + beta_long_signal;

    // ========================================================================
    // Wrappable Syntax Structure tests (1-7 from Formatter_wrap.cpp)
    // ========================================================================

    // ---- Structure 1: BinaryExpression (arithmetic, logical, bitwise) ----
    // Arithmetic chain
    assign result = alpha_long_signal + beta_long_signal + gamma_long_signal
                    + delta_long_signal + epsilon_long_signal;

    // Logical chain
    assign output_val = condition_long_flag || enable_long_flag || ready_long_flag
                        || (alpha_long_signal > beta_long_signal);

    // Bitwise chain
    assign result = alpha_long_signal | beta_long_signal | gamma_long_signal
                    | delta_long_signal | epsilon_long_signal;

    // Shift expression
    assign result = alpha_long_signal + (beta_long_signal << gamma_long_signal)
                    + (delta_long_signal >> epsilon_long_signal);

    // Comparison
    always_comb begin
        output_val = (alpha_long_signal >= beta_long_signal)
                     && (gamma_long_signal <= delta_long_signal)
                     && (epsilon_long_signal != zeta_long_signal);
    end

    // ---- Structure 2: AssignmentExpression (=, +=, <=) ----
    // Blocking assignment
    always_comb begin
        result = alpha_long_signal + beta_long_signal + gamma_long_signal
                 + delta_long_signal + epsilon_long_signal;
    end

    // Non-blocking assignment
    always_ff @(posedge clk) begin
        result <= alpha_long_signal + beta_long_signal + gamma_long_signal
                  + delta_long_signal + epsilon_long_signal;
    end

    // Compound assignment
    always_comb begin
        result += alpha_long_signal + beta_long_signal + gamma_long_signal
                  + delta_long_signal + epsilon_long_signal;
    end

    // ---- Structure 3: ConditionalExpression (ternary) ----
    // Simple ternary
    assign result =
        condition_long_flag ? alpha_long_signal + beta_long_signal
                                 + gamma_long_signal : delta_long_signal + epsilon_long_signal;

    // Nested ternary
    assign result =
        condition_long_flag
            ? enable_long_flag
                ? alpha_long_signal
                : beta_long_signal
            : gamma_long_signal;

    // ---- Structure 4: ConcatenationExpression ----
    // Long concatenation goes vertical via Dynamic list (not expression wrapping)
    assign wide_concat = {
        alpha_long_signal,
        beta_long_signal,
        gamma_long_signal,
        delta_long_signal,
        epsilon_long_signal
    };

    // Concatenation as assignment RHS (should NOT wrap at =)
    assign concat_result = {
        alpha_long_signal,
        beta_long_signal,
        gamma_long_signal,
        delta_long_signal,
        epsilon_long_signal
    };

    // ---- Structure 5: InvocationExpression ----
    // Long sole-RHS call moves to the next indented line
    assign result =
        compute(
            alpha_long_signal + beta_long_signal + gamma_long_signal,
            delta_long_signal + epsilon_long_signal
        );

    // ---- Structure 6: ContinuousAssign ----
    // assign with long expression (wrapping happens in the expression)
    assign result = alpha_long_signal + beta_long_signal + gamma_long_signal
                    + delta_long_signal + epsilon_long_signal;

    // ---- Structure 7: DataDeclaration with initializer ----
    // Variable declaration with long initializer expression
    logic [31:0] initialized_var = alpha_long_signal + beta_long_signal + gamma_long_signal
                                   + delta_long_signal + epsilon_long_signal;

    // ---- Short expressions that fit inline (no wrapping) ----
    assign result = alpha_long_signal + beta_long_signal;
    assign output_val = condition_long_flag ? alpha_long_signal : beta_long_signal;

    always_comb aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa =
        bbbbbbbbbbbbbbbbbbb;

    assign foo = (bar & baz)         ? (something + 17'd42) :
                 (oh_crap | its_bad) ? (a & b) + myfunc(e, f[2:0]) :
                                       10'h1;

endmodule
