// Conditional directives inside a recovered macro assignment remain lossless.
module m;
    `ASSIGN_COMB(item_t, item) = '{
        value: source,
`ifdef WIDE_ITEM
        size: 3,
`else
        size: 2,
`endif
        payload: data
    };
endmodule
