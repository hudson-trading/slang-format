module task_function;
    logic clk;
    logic [7:0] result;

    function automatic logic [7:0] add_saturate(
        input logic [7:0] a,
        input logic [7:0] b
    );
        logic [8:0] sum;
        sum = a + b;
        if (sum[8])
            return 8'hFF;
        else
            return sum[7:0];
    endfunction

    task automatic wait_cycles(input int n);
        repeat (n) @(posedge clk);
    endtask

    function automatic logic [7:0] recursive_fib(input int n);
        if (n <= 1)
            return n[7:0];
        else
            return recursive_fib(n - 1) + recursive_fib(n - 2);
    endfunction

    initial begin
        result = add_saturate(8'hF0, 8'h20);
        wait_cycles(10);
        result = recursive_fib(8);
    end
endmodule
