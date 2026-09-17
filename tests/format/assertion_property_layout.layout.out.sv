// Long concurrent assertions separate the clock/disable clause from the checked expression.
module demo;
    `ifdef FEATURE_A
        check_value: assert property (
            @(posedge baz) disable iff(flag0)
            some_function_a(some_input_a) == some_function_a(second_input_b)
        );
    `endif

    // The label and surrounding indentation count toward the line limit.
    `ifdef FEATURE_B

        property_a: assert property (
            @(posedge baz) disable iff(bar)
            some_other_function(foo) == some_other_function(bar_baz)
        );

    `endif

    check_clock: assert property (
        @(posedge baz)
        some_function_a(some_input_a) == some_function_a(second_input_b)
    );
    check_reset: assume property (
        disable iff(flag0)
        some_function_a(some_input_a) == some_function_a(second_input_b)
    );
    check_plain: assert property (some_function_a(some_input_a) == some_function_a(second_input_b));
    check_long_plain: assert property (
        some_function_a(some_long_signal_a) == some_function_a(another_long_input_b)
    );
    check_cover: cover property (
        @(posedge baz) disable iff(flag0)
        some_function_a(some_input_a) == some_function_a(second_input_b)
    );
    check_sequence: cover sequence (
        @(posedge baz)
        some_function_a(some_input_a) ## 1 some_function_a(second_input_b)
    );
    check_restrict: restrict property (
        @(posedge baz) disable iff(flag0)
        some_function_a(some_input_a) == some_function_a(second_input_b)
    );
    initial expect (
        @(posedge baz) disable iff(flag0)
        some_function_a(some_input_a) == some_function_a(second_input_b)
    );
    check_action: assert property (
        @(posedge baz) disable iff(flag0)
        some_function_a(some_input_a) == some_function_a(second_input_b)
    ) else $error("mismatch");
    check_implication: assert property (
        @(posedge baz) disable iff(flag0)
        some_signal_a |-> some_signal_b == some_signal_c
    );

    check_comments: assert property (  // Check the sampled values.
        @(posedge baz) disable iff(flag0)  // Ignore disabled cycles.
        some_function_a(some_input_a) == some_function_a(second_input_b)  // Compare call results.
    );

    // Short concurrent and immediate assertions stay compact.
    short_check: assert property (@(posedge baz) disable iff(flag0) a == b);
    always_comb assert (a == b);
endmodule
