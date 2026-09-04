// Consecutive macros that expand to covergroup members must stay on separate
// lines from each other and from a following explicit cross declaration.
`define VALUE_COVERPOINT \
    value_cp: coverpoint value;

`define STATUS_COVERPOINT \
    status_cp: coverpoint status;

covergroup sample_group with function sample(int value, int status);
    `VALUE_COVERPOINT
    `STATUS_COVERPOINT
    value_status_cross: cross value_cp, status_cp;
endgroup
