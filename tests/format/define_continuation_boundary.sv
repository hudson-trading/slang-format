// A continuation consumes one newline, not later blank or comment-only lines.
`ifndef HEADER
`define HEADER
`define FIRST(x) \
    call(x); \

`define SECOND(x) \
    other(x); \
// This comment ends the definition.

`define THIRD(x) \
    third(x);
`endif
