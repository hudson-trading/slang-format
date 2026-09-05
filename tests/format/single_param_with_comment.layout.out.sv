// Single-item param/port lists collapse to inline (`#(.WIDTH(8))`), but
// any leading line/block comments on the first item MUST survive — only
// whitespace and EOL trivia get suppressed by the inline collapse.

module top;
    // Leading line comment on the single param.
    child #(
        // explains why this stage uses VALID-only handshake
        .stage_type(PIPELINE_VALID)
    ) u_a (.clk(clk));

    // Leading block comment on the single param.
    child #(
        /* explanatory note */
        .stage_type(PIPELINE_VALID)
    ) u_b (.clk(clk));

    // Leading line comment on the single port-connection.
    child #(.WIDTH(8))
      u_c (
          // wired straight through
          .clk(clk)
      );

    // No comment, single param — should collapse fully.
    child #(.WIDTH(8))
      u_d (.clk(clk));
endmodule
