// Named port connections with varying name lengths
module top;
  sub u_sub (
    .a(sig_a),
    .long_name(sig_b),
    .x(sig_c),
    .mid_len(sig_d)
  );

  // Named param assignments with varying name lengths
  sub #(
    .WIDTH(8),
    .DEPTH_OVERRIDE(16),
    .N(4)
  ) u_sub2 (
    .clk(clk),
    .data_in(data),
    .rst(reset)
  );

  // Blocking assignments with varying LHS lengths
  always_comb begin
    a = 1;
    long_variable = 2;
    x = 3;
    mid_var = 4;
  end

  // Nonblocking assignments
  always_ff @(posedge clk) begin
    q <= d;
    long_output <= long_input;
    x <= y;
  end
  // A single blank line does NOT split port alignment groups (port/param
  // groups require 2 blank lines to split — see port_param_grouping.sv).
  sub u_split (
    .clk(clk),
    .data_in(data),

    .rst(reset),
    .very_long_port_name(sig)
  );

  // With default paddingLimit = null, wide gaps keep the group aligned.
  // Set alignment.paddingLimit to a positive integer N to split the group
  // when a column needs N or more spaces of padding.
  sub u_max_spaces (
    .a(x),
    .this_port_name_is_over_thirty_chars_longer(y),
    .b(z)
  );
  // Assignments with similar LHS but very different RHS lengths still align
  always_comb begin
    resp_comb.read_ack = '1;
    resp_comb.read_data = SomePkg_pkg::bus_data_t'(value_internal);
    resp_comb.read_error = '0;
  end

  // Nonblocking with varying RHS complexity
  always_ff @(posedge clk) begin
    status_reg.valid <= enable_in;
    status_reg.data <= compute_result(alpha_input, beta_input, gamma_input);
    status_reg.error <= '0;
  end

  // Assignments where one RHS wraps (ternary) — still aligns at =
  always_comb begin
    if (num_bytes > 1) begin
      produce_decoded = '1;
      decoded_next.eop = term_next < num_bytes;
      decoded_next.empty = (term_next >= num_bytes) ? empty_t'(1) : empty_t'(num_bytes - term_next);
    end
  end
endmodule
