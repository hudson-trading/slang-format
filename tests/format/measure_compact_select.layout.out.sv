// Compact index expressions inside `[...]` have no whitespace around
// operators in the formatted output (compactExpr). The pre-refactor
// estimate counted these with hypothetical spaces, which overshot the
// real emitted width and produced spurious wrapping decisions.
// measureInlineWidth runs the actual formatting code path, so it sees
// the compact spacing and keeps these lines inline.
module test;
    logic [255:0] mem;
    logic [31:0] result;
    logic [31:0] ix_alpha, ix_beta, ix_gamma, ix_delta, ix_epsilon, ix_zeta;

    initial begin
        // 5 `+`s inside `[...]` save 10 chars vs an estimate that added
        // spaces — that's enough slack to keep this line under 100 cols.
        result = mem[ix_alpha+ix_beta+ix_gamma+ix_delta+ix_epsilon+ix_zeta] - 32'hABCDEF12345;
    end
endmodule
