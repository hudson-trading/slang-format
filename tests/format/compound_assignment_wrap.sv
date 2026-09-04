// Compound assignments wrap after the operator when the full statement would
// exceed the column limit, while short compound assignments stay inline.
module compound_assignment_wrap;
  logic [127:0] destination_with_a_long_descriptive_name;
  logic [127:0] source_with_another_long_descriptive_name;
  logic [127:0] short_a;
  logic [127:0] short_b;

  always_comb begin
    destination_with_a_long_descriptive_name[127:64] |= source_with_another_long_descriptive_name[127:64];
    short_a |= short_b;
  end
endmodule
