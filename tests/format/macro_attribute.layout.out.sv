module macro_attr_test;
    `PIPE(clk, in_a, out_a, 0, lat)
    (* `NO_MERGE, `FANOUT(16) *)
    `PIPE(clk, in_b, out_b, 0, lat)
    (* `NO_MERGE, `FANOUT(16) *)
    `PIPE(clk, in_c, out_c, 0, lat)
    `PIPE(clk, in_d, out_d, 0, lat)
endmodule
