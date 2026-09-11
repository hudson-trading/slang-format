// Wrapping a call must not change macro ownership and its available break points.
task foo();
    check_value(
        0,
        `VALUE(very_long_scope.some_really_long_assertion_name_that_exceeds_the_configured_column_limit)
    );
endtask
