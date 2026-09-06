// A standalone block comment before the first parameter must not exclude that row from alignment.
module foo #(
    /* tool annotation */
    parameter int  item_count,
    parameter int  lane_count,
    parameter type item_t     = logic [item_count-1:0]
) ();
endmodule
