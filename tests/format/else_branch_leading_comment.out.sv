module top;
    always_ff @(posedge clk) begin
        if (generate_multi_proc) begin
            proc_event_cnt_ff <= committed_set_cnt;
            extend_final_ff   <= (committed_set_cnt == 'd1);

        // 1-of-many proc mode, flag the single output we intend to emit
        end else begin
            proc_event_cnt_ff      <= sink_output_loc_idx_found;
            sink_output_loc_sel_ff <= 1 << sink_output_loc_idx;
        end
    end

    generate
        if (W > 0) begin : g_pos
            logic flag;

        // Fallback implementation for zero-width instances
        end else begin : g_zero
            logic flag2;
        end
    endgenerate
endmodule
