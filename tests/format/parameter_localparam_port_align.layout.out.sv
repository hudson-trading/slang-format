// Parameter and localparam entries in one parameter-port list align the
// keyword/type boundary as well as the name and value columns.
module parameter_localparam_port_align #(
    parameter type data_t = logic [31:0],
    parameter int depth = 16,
    parameter int skip_width = $clog2(depth + 1),
    localparam int level_width = $clog2(depth + 1)
) ();
endmodule
