// Conditional directives that share a line (`else `ifdef, `endif `endif)
// format like the same directives written one per line.

module m;
`ifdef A
  initial $display("a");
`else `ifdef B
  initial $display("b");
`else
  initial $display("c");
`endif `endif
endmodule
