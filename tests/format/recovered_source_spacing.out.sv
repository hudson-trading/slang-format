// Parser recovery of a reserved block name must remain stable.
if (VALUE == "TRUE") begin : soft
    reg  [N-1:0] memory[0:DEPTH-1];
    wire [N-1:0] value;
