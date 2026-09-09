// Multiline macro arguments follow the call's indentation in either branch.
module foo;
    `ifdef FEATURE
        `DECLARE_MONITOR(
            first_mon,
            first_data,
            data_t,
            first_data.clk,
            first_data.rst
        )

        `DECLARE_MONITOR(
            second_mon,
            second_data,
            data_t,
            second_data.clk,
            second_data.rst
        )
    `else
        `DECLARE_MONITOR(
            third_mon,
            third_data,
            data_t,
            third_data.clk,
            third_data.rst
        )
    `endif

    `DECLARE_MONITOR(
        fourth_mon,
        fourth_data,
        data_t,
        fourth_data.clk,
        fourth_data.rst
    )

    `ifdef OUTER
        `ifdef INNER
            `CHECK(
                foo,
                bar
            )
        `endif
    `endif

    `CHECK(
        // Leading comments follow the argument.
        foo,

        {bar, baz},
        /* Keep the interior
    of this comment. */
        qux
        // Closing comment.
    )

    // Whitespace within an argument can affect stringification.
    initial $display(`STRINGIFY(
                         first  word
   second word
                     ));
endmodule
