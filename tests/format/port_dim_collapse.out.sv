// Port-list alignment: if any row lacks packed dims, collapse the dim
// sub-column so the dim-less rows don't pad through a phantom bracket
// column. The dim column is only preserved when EVERY row in the group has
// packed dims (so `[7:0]` / `[15:0]` cleanly line up). Mixing dim-less and
// dimmed rows aligns each row's whole type+dim as one unit against the
// widest one.

module port_dim_mixed (
    // Some rows have dims, others don't -> dim column collapsed, each row's
    // type+dim packs tightly, identifiers align at the longest combined
    // width.
    input alpha_t [1:0] sig_a,
    input beta_t        sig_b,
    input time_t        sig_c,
    input logic         ctrl_pause_a,
    input logic         ctrl_pause_long_b,
    input logic         ctrl_sink_valid,
    input ctrl_meta_t   ctrl_sink_meta
);
endmodule

module port_dim_mixed_keep (
    // Mixed: one row has no dims. Dim column collapses for all rows so each
    // row's type+dim sits compact on the left, and identifiers align past
    // the longest combined type+dim.
    input  logic                 clk,
    input  SomePkg::some_t [7:0] a,
    output logic [7:0]           b,
    output logic [15:0]          c
);
endmodule

module port_dim_all (
    // Every row has packed dims -> dim column preserved, `[7:0]` and
    // `[15:0]` align across rows.
    input  logic [7:0]  data_in,
    output logic [7:0]  data_out,
    output logic [15:0] addr_out
);
endmodule
