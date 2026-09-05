// Multi-instance declarations (`Type a (...), b (...);` and the param
// variant `Type #(...) a (...), b (...);`) lay out each instance name
// on its own line at parent_indent + 2 — but only when at least one
// instance has port connections.
//
// Rules:
//   * Multiple instances, all with empty port lists (`Type a, b;`) →
//     keep inline. There's nothing to scan past.
//   * Multiple instances, any has connections → always break, even when
//     the whole decl fits on one line. Readability beats compactness
//     when each instance has its own `(...)` block.
//   * Single inline param (`Type #(.x(y))`) moves the FIRST instance
//     to its own indented line too (at parent + 2).
//   * Vertical (multi-row) params + multi-instance → first instance
//     follows `) `, subsequents at parent + 2.

module top;
  // Short decl with connections — ALWAYS breaks across lines because
  // each instance has its own port-connection block, even though the
  // whole decl fits well under the column limit.
  T a (.x(x)), b (.y(y));

  // Same Type, no connections — stays inline.
  T x, y;

  // Short single-param + two instances with connections.
  // Both instance lines must break.
  Filter #(.symbols_per_beat(pkg_x::symbols_per_beat)) u_mon_a (.clk(clk_a), .rst(rst_a)), u_mon_b (.clk(clk_a), .rst(rst_a));

  // No params, long type name — subsequent at parent+2, NOT under the
  // first instance.
  VeryLongTypeName first_instance (.clk(clk), .rst(rst)), second_instance (.clk(clk), .rst(rst)), third (.clk(clk));

  // Inline single-param + multi-instance — verifies the second instance
  // name lands at the SAME column as the first (no extra drift).
  StreamHolder #(.data_t(data_t)) holder_a (.clk(bus.clk), .rst(bus.rst)), holder_b (.clk(clk_full), .rst(rst_full));

  // Opening parens align across differently sized instance names.
  Channel #(.items_per_word(stream_items_per_word)) short (.clk(clk), .rst(rst)), medium_instance (.clk(clk), .rst(rst)), longest_instance_name (.clk(aux_clk), .rst(aux_rst));

  // Vertical (multi-row) params + multi-instance.
  Type #(.W(8), .D(16), .E(32), .F(64)) a_inst (.clk(clk), .rst(rst), .x(x), .y(y)), b_inst (.clk(clk), .rst(rst), .x(x), .y(y));

  // Single instance with no params — stays on one line.
  Single u_only (.clk(clk));
endmodule
