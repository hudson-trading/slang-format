// Standalone comments contribute N-1 separator lines to alignment grouping.
// These one-line interface-port annotations therefore contribute zero; only
// the single actual blank line in each gap counts toward the two-line split
// threshold.
module single_comment_line_is_free (
    // meta: first_meta_t
    StreamBus.sink    sink,

    // meta: second_meta_t
    StreamBus.source  source,

    // meta: monitor_meta_t
    StreamBus.monitor stream_monitor,

    ControlBus.slave  control
);
endmodule

// Contrast: two comment lines contribute one separator. Together with the
// actual blank line above them, they split this structural alignment group.
module two_comment_lines_plus_blank_split (
    short_iface.sink first,

    // First comment line.
    // Second comment line.
    substantially_longer_iface.source second
);
endmodule

// The same two comment lines without a real blank contribute only one
// separator, which is not enough to split a structural group.
module two_comment_lines_alone_do_not_split (
    short_iface.sink                  first,
    // First comment line.
    // Second comment line.
    substantially_longer_iface.source second
);
endmodule

// A two-line block comment follows the same rule: it contributes one separator
// and combines with the actual blank line to split the group.
module two_line_block_comment_plus_blank_splits (
    short_iface.sink first,

    /* First comment line.
   * Second comment line. */
    substantially_longer_iface.source second
);
endmodule

// Two actual blank lines still split without help from a comment.
module two_blanks_still_split (
    short_iface.sink first,


    // This one-line annotation contributes zero.
    substantially_longer_iface.source second
);
endmodule
