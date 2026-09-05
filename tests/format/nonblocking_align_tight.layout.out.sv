// Per-row alignment speculation: each row in an alignment group is
// speculatively emitted in both modes (aligned vs standalone, no
// alignment). The mode with fewer overflow lines wins, with total line
// count as tie-break, and aligned as final default. When alignment would
// push `<=` so far right that the RHS chain wraps off the column limit,
// the row is emitted standalone instead.

module test;
    always_ff @(posedge clk) begin
        if (en) begin
            // Two nonblocking assigns with very different LHS widths.
            // The wider LHS would push the aligned `<=` far right; with
            // the tight threshold the group splits and each gets its own
            // natural `<=` position.
            pipeline_state_inside.data.counter.remaining <=
                pipeline_state.data.counter.remaining == '0 ? '0 :
                !pipeline_state.data.is_last                ? amount_per_step :
                cancel_first_step                           ? pipeline_state.data.counter.remaining :
                                                              amount_remaining;
            amount_remaining <=
                pipeline_state.data.counter.remaining == '0 ? '0 :
                pipeline_state.data.is_last                 ? 'x :
                cancel_first_step                           ? pipeline_state.data.counter.remaining
                                                              - amount_per_step :
                                                              amount_remaining - amount_per_step;
        end
    end

    // Contrast: similar LHS widths still align.
    always_ff @(posedge clk) begin
        if (en) begin
            data_out_a <= some_input_signal_a;
            data_out_b <= some_input_signal_b;
            data_out_c <= some_input_signal_c;
        end
    end
endmodule
