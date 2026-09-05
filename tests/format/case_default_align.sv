// Regression: `default:` in a case
// statement should participate in the surrounding alignment group, not
// land flush against `:` with no padding. Previously DefaultCaseItem
// produced no AlignRow and was emitted directly, so its `:` did not
// align with the standard items' colons.

module test;
    always_ff @(posedge clk) begin
        case (msgid)
            MODE_ALPHA       : action_out <= act_a;
            MODE_BETA         : action_out <= act_b;
            MODE_GAMMA_EXTENDED: action_out <= act_c;
            MODE_DELTA         : action_out <= act_d;
            default               : action_out <= action_out;
        endcase
    end

    // `default` is the longest label — other labels pad up to its width.
    logic match;
    always_comb begin
        case (opcode) inside
            [0:3]  : match = 1'b1;
            [8:15] : match = 1'b1;
            default: match = 1'b0;
        endcase
    end
endmodule
