// Test: breaking a case-item clause onto its own line after the `:` is
// all-or-none across the WHOLE case statement. Two triggers, one rule:
//
// f(): the aligned-group overflow path. One arm's aligned clause overflows
// the column; every arm in the case breaks so there's no mix of inline
// (`AA: x = 1;`) and broken (`WIDE:\n    x = ...`) arms.
//
// h(): the cross-group path. A wide multi-label arm is rejected from
// alignment entirely (its labels stack one-per-line) and breaks its clause;
// the single-label arms live in a separate alignment group but still break
// their clauses to match. The per-group syncs can't see across groups, so
// this is decided case-wide in formatVerticalList.
//
// n(): scope isolation. The decision is per-case-statement, not per-file.
// The outer case has a wide arm and breaks, but the INNER case (inside the
// block-body arm) has short arms that all fit — it must stay inline/aligned.
// The break signal must not leak across the nested-case boundary.
module m;
    function automatic int f(int sel);
        case (sel)
            AA: x = 1;
            MODERATELY_LONGISH_LABEL_NAME: x = some_value_that_together_overflows_the_column_limit_x;
            BB: x = 2;
        endcase
    endfunction

    function automatic int h(int sel);
        case (sel)
            aa: y = 1;
            bb: y = 2;
            LongLabelPkg::WIDE_A, LongLabelPkg::WIDE_B, LongLabelPkg::WIDE_C: y = 3;
            cc: y = 4;
        endcase
    endfunction

    function automatic void n(int outer, int inner);
        case (outer)
            WideLabelPkg::OUTER_A, WideLabelPkg::OUTER_B, WideLabelPkg::OUTER_C: y = 1;
            other: begin
                case (inner)
                    d0: z = a;
                    d1: z = b;
                    default: ;
                endcase
            end
        endcase
    endfunction

    // Contrast: when nothing overflows and no arm is rejected from alignment,
    // all arms stay inline and aligned.
    function automatic int g(int sel);
        case (sel)
            AA: y = 1;
            BBBB: y = 2;
            C: y = 3;
        endcase
    endfunction
endmodule
