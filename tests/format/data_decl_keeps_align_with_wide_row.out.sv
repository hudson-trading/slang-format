// Data-declaration alignment is purely cosmetic (lines up the type
// column) and should stay aligned across the group even if one row's
// packed-dim / name pushes the line past the column limit. The
// alignment speculation only applies to assignment-like rows
// (ExpressionStatement, ParameterDeclarationStatement) where breaking
// after `=`/`<=` actually reshapes the RHS.

module test;
    logic                    sample_valid;
    vector_pkg::branch_ids_t branch;
    vector_pkg::client_id_t  client;
    vector_pkg::sample_t     samples[vector_pkg::NUM_SAMPLES-1:0];
endmodule
