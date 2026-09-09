// Directive recovery must preserve source spelling and macro include arguments.
`define HEADER "file.svh"
`include `HEADER
`unconnected_drive pull2
`unconnected_drive
module foo; endmodule
