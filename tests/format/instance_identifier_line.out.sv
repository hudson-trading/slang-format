module instance_identifier_line;
    // No parameters, with vertical connections.
    QueueWriter
      queueWriter (
        .control (control),
        .write   (fifo_write),
        .ready   (ready)
    );

    // Inline parameter override.
    Child #(.WIDTH(8))
      u_byte (.in(a), .out(b));

    // Vertical parameter overrides.
    Child #(
        .WIDTH  (16),
        .SIGNED (1)
    ) u_word (
        .in    (a),
        .out   (b),
        .ready (ready)
    );

    // Multiple empty instances can share their identifier line.
    Leaf
      first (), second ();

    // Non-instantiation identifiers remain inline with their types.
    logic data;
endmodule
