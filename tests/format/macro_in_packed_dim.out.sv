// When a macro stands in for an expression operand inside a packed dim
// (e.g., `[`WIDTH - 1 : 0]` or `[ (X==`KIND_A)*(`MULT*3) ]`), slang's
// parser produces an empty Identifier placeholder carrying the macro as
// Directive trivia. The port alignment producer must preserve those
// placeholders (instead of skipping all empty tokens) so the macro
// survives into the formatted output; otherwise the CST diverges from
// the input.

module dut #(
    parameter integer P = 0,
    parameter integer M = 0
) (
    input wire                           clk,
    input wire [M*3-1:0]                 dat,
    input wire [`SOME_MACRO - 1 : 0]     dat2,
    input wire [P + `SOME_MACRO - 1 : 0] dat3,
    input wire [((P == `KIND_A) * (1)) +
                ((P == `KIND_B) * (`MULT * 3)) - 1 : 0] dat4
);
endmodule
