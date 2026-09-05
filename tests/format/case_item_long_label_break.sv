// Breaking a case-item clause onto its own indented line is all-or-none
// across the WHOLE case statement, not just within one alignment group.
// When any non-block arm's clause breaks (here OP_VAL_LONGISH crosses the
// readability threshold, constants::maxInlineCaseItemLabelWidth = 30), every
// non-block arm breaks too — including short-labeled arms and arms in
// separate alignment groups (split out by the block-body arm). Block bodies
// (begin/end) always stay attached to their label regardless.

module test;
    always_comb begin
        unique case (op)
            // One arm (OP_VAL_LONGISH) crosses the threshold, so every
            // non-block arm in the case gets clause-on-next-line — even the
            // short OP_READ and the `default` below.
            OP_READ: y = 1;
            ThisPackage_pkg::OP_VAL_FOO: y = 2;
            ThisPackage_pkg::OP_VAL_LONGISH: y = 3;
            OP_FOO, OP_BAR, OP_BAZ, OP_QUX, OP_QUUX: y = 4;
            // Block bodies stay attached regardless of label width.
            ThisPackage_pkg::OP_VAL_LONGER_NAME: begin
                y = 5;
            end
            // Short label, but the whole-case sync still breaks its clause.
            default: y = 0;
        endcase
    end
endmodule
