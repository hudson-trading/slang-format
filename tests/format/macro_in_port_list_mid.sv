module top;
    logic in_signal;
    logic out_signal;
    some_module #(
        .func(2),
        .width(1)
    ) inst1 (
        `EXTRA_PORTS_BIND(clk, 1'b0),
        .in(in_signal),
        .out(out_signal)
    );
endmodule
