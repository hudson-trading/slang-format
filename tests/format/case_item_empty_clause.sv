// An empty case-item clause always renders as `LABEL: ;` / `default: ;`:
// the `;` hugs the colon (one space) and never breaks onto its own line,
// even when the whole-case clause-break sync fires because a sibling arm's
// clause breaks. Breaking a bare `;` to its own indented line is pure noise.
module m;
    // Sync fires (wide multi-label arm breaks), but the empty clauses stay
    // inline as `: ;`.
    always_comb begin
        case (a)
            WidePkg::LABEL_A, WidePkg::LABEL_B, WidePkg::LABEL_C: x = 1;
            bb: ;
            default: ;
        endcase
    end

    // No sync: aligned arms, empty clause still `: ;`.
    always_comb begin
        case (b)
            cc: y = 1;
            dd: y = 2;
            default: ;
        endcase
    end
endmodule
