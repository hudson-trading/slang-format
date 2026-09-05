// Port/param alignment groups require TWO blank lines to split (a single
// blank line is preserved but keeps the group aligned together). Also
// verifies the longest name in a group still gets at least one space
// before its `(`.

module top;
    // Single blank line: all four port connections stay in one aligned group;
    // .very_long_port_name gets exactly one space before `(`.
    sub
      u_one_blank (
        .clk                 (clk),
        .data_in             (data),

        .rst                 (reset),
        .very_long_port_name (sig)
    );

    // Two blank lines: groups split, so each side aligns independently.
    sub
      u_two_blanks (
        .clk     (clk),
        .data_in (data),


        .rst                 (reset),
        .very_long_port_name (sig)
    );

    // Param assignments behave the same way.
    sub #(
        .WIDTH (8),
        .DEPTH (16),


        .N               (4),
        .EXTRA_LONG_NAME (1)
    ) u_split_params ();
endmodule
