// Alignment partitions separated by blank lines remain stable after normalization.
`include "defs.svh"

module m /* verilator public_on */
#(
    parameter  int  first_count,
    parameter  int  second_count,

    parameter  type item_t,

    localparam int  item_width = $bits(item_t),
    localparam type word_t     = logic [item_width-1:0]
) (
    short_bus.sink   first,

    longer_bus.source second,
    short_bus.source  third,

    short_bus.source fourth
);
    `CHECK()
endmodule
