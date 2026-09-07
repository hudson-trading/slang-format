// Comments inside conditional directives survive in active and inactive branches.
module foo;
    `ifdef /* feature selection */ FEATURE
        logic selected;
    `elsif ( OTHER /* alternate selection */ || THIRD )
        logic alternate;
    `else
        logic fallback;
    `endif
endmodule
