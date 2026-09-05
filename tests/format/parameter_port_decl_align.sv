// Parameter-port declarations align keyword, type, name, equals, and value
// columns across mixed value and type parameters. Missing defaults leave the
// equals/value columns empty without breaking the group.

module parameter_port_decl_align #(
  parameter type metadata_t,
  parameter int storage_depth,
  parameter type payload_t = logic [DATA_WIDTH-1:0],
  parameter config_pkg::very_wide_mode_t operating_mode,
  parameter int jump_table_latency = 2,
  parameter bit force_single_mode = 0,
  parameter int num_output_sets = 2, // trailing comment remains attached
  parameter int trace_depth = 256
) ();
endmodule

// Class type-parameter lists can omit the `parameter` keyword entirely. The
// empty keyword column must not introduce a leading space before `type`.
class parameter_port_implicit_keyword #(
  type PAYLOAD_T,
  type WR_IDX_T = int,
  type RD_IDX_T = int
);
endclass

// A multi-declarator parameter is deliberately left on the generic path: one
// syntax row contains two declarations, so it cannot participate in the
// one-declaration-per-row table without rearranging source structure.
module parameter_port_multi_decl #(
  parameter int FIRST = 1, SECOND = 2,
  parameter int THIRD = 3
) ();
endmodule

// An uninitialized value parameter does not pad its name to align with a
// separate run of initialized local type parameters.
module parameter_port_mixed_local #(
  parameter int word_width,
  localparam type index_t = logic [$clog2(word_width)-1:0],
  localparam type raw_t = logic [word_width-1:0],
  localparam type user_t = logic [word_width-2:0]
) ();
endmodule

// A blank-line boundary can split a parameter-port alignment group when a wide
// type in the earlier section would otherwise force later derived calls to wrap.
module parameter_port_section_split #(
  parameter int input_count,
  parameter storage_pkg::very_wide_storage_mode_t storage_mode,
  parameter bit use_storage,

  // Derived values
  parameter int branch_width = maximum(1, log_two(branch_count)),
  parameter int inputs_per_cycle = math_pkg::ceil_divide(input_count, folding_cycles),
  parameter int outputs_per_cycle = math_pkg::ceil_divide(output_count, streaming_cycles)
) ();
endmodule

// Contrast: section gaps keep sharing columns when alignment does not make any
// parameter wrap.
module parameter_port_sections_stay_aligned #(
  parameter int width,

  parameter string description,
  parameter string display_format = "%u",
  parameter bit enabled = 0,

  parameter bit writable = 1,
  parameter bit saturating = 1,

  // Derived value
  parameter type data_t = logic [width-1:0]
) ();
endmodule
