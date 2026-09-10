// Single-line macro arguments use normal width-based wrapping.
task run();
    check_path(0, `PATH(`FIFO_A, data_known));
    check_path(0, `PATH(`FIFO_B, scope.depth_within_limit));

    // Bare macros remain vertical because they can stand for arbitrary syntax.
    check_path(
        0,
        `PATH_NAME
    );

    // Multiline macro invocations retain their internal line structure.
    check_path(
        0,
        `PATH(
            `FIFO_A,
            data_known
        )
    );

    // A call that exceeds the column limit still wraps.
    check_path(
        0,
        `PATH(`FIFO_C, deeply_nested_scope.some_really_long_assertion_name_that_exceeds_limit)
    );
endtask
