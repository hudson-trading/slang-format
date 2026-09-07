// Inactive block comments may be reindented without changing comment contents.
module foo;
    `ifdef FEATURE
        logic a;
        /* description
    detail
 */
    `else
        logic b;
    `endif
endmodule
