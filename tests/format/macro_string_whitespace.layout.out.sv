// Macro quoting and continued strings preserve their internal whitespace.
`define MESSAGE `"first \
                     second`"
`define TEXT "first\
  second"
`define STRINGIFY(x) `"x`"
module foo;
    initial $display(`MESSAGE);
    initial $display(`TEXT);
    initial $display(`STRINGIFY(first
                      second));
    `ifdef FEATURE
        initial $display(`STRINGIFY(first
                      second));
    `endif
endmodule
