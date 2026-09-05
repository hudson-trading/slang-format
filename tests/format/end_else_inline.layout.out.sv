// `end` stays on the same line as the following `else` — `end else if`,
// `end else begin`, `end else <stmt>`, plus the generate if/else mirrors.
// Reads more naturally than splitting onto two lines, and matches what
// most projects already do by hand.

module top;
    initial begin
        // Plain else
        if (a) begin
            x = 1;
        end else begin
            x = 2;
        end

        // else-if chain
        if (a) begin
            x = 1;
        end else if (b) begin
            x = 2;
        end else begin
            x = 3;
        end

        // Empty blocks
        if (a) begin end else begin end

        // else single statement (not a block) — also lands on its own line.
        if (a) begin
            x = 1;
        end else
            x = 2;
    end

    // Generate if/else
    generate
        if (W > 0) begin : g_pos
            logic flag;
        end else begin : g_neg
            logic flag2;
        end

        // Generate else-if
        if (W > 8) begin : g_wide
            logic w1;
        end else if (W > 4) begin : g_med
            logic w2;
        end else begin : g_narrow
            logic w3;
        end
    endgenerate
endmodule
