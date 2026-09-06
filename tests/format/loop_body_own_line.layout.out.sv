// A loop body without begin/end always starts on the following indented line.
module loop_body_own_line;
    always_ff @(posedge clk) begin
        for (int index = depth - 1; index > 0; index--)
            stages[index] <= stages[index-1];
    end
endmodule
