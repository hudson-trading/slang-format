module fork_join;
    logic clk, done_a, done_b, done_c;

    initial begin
        // Basic fork-join
        fork
            begin
                @(posedge clk);
                done_a = 1;
            end
            begin
                @(posedge clk);
                @(posedge clk);
                done_b = 1;
            end
        join

        // fork-join_any
        fork
            begin : task_a
                repeat (5)
                    @(posedge clk);
                done_a = 1;
            end
            begin : task_b
                repeat (3)
                    @(posedge clk);
                done_b = 1;
            end
        join_any
        disable fork;

        // fork-join_none
        fork
            begin
                forever
                    @(posedge clk) done_c = ~done_c;
            end
        join_none

        // Nested fork
        fork
            fork
                @(posedge done_a);
                @(posedge done_b);
            join
            @(posedge done_c);
        join_any

        wait fork;
    end
endmodule
