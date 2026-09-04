// Two shorthand named connections stay inline on each instance even when a
// multi-parameter header and multiple instances make the declaration vertical.
module multi_instance_implicit_connections_inline;
    Pair #(
        .key_width($bits(item_t)),
        .value_width(1)
    ) first_pair[2]  (.clk, .rst),
      second_pair[2] (.clk, .rst);

    // Wildcard connections are shorthand too and do not make the declaration vertical.
    Pair #(
        .key_width($bits(item_t)),
        .value_width(1)
    ) implicit_first[2] (.clk, .rst),
      wildcard_second   (.*);

    // Contrast: explicit connections retain the vertical connection layout.
    Pair #(
        .key_width($bits(item_t)),
        .value_width(1)
    ) explicit_first[2]  (
        .clk(clk),
        .rst(rst)
    ),
      explicit_second[2] (
        .clk(clk),
        .rst(rst)
    );
endmodule
