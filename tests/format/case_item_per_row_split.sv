// Test: multiple case-item labels split one-per-line when the combined
// label width exceeds the short threshold, even if they fit in the column.
// Independently, once any arm's clause breaks (the wide MODE arm here),
// every arm's clause breaks (whole-case sync) — but the LABEL-split axis is
// still distinct: the wide labels stack one-per-line while short label runs
// (`A, B:`) stay together on one line.

module test;
    always_comb begin
        case (mode)
            // Combined labels wider than threshold -> labels one per line
            MODE_HIGH_THROUGHPUT, MODE_LOW_LATENCY: handler = 1;
            // Short combined labels stay together on one line (clause still
            // breaks due to the whole-case sync)
            A, B: y = 1;
            // Single label never stacks (clause still breaks via the sync)
            MODE_HIGH_THROUGHPUT: handler = 2;
            default: handler = 0;
        endcase
    end
endmodule
