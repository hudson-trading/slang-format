// Alignment must not push packed declarations beyond the column limit.
module demo;
    logic                  [example_pkg::capacity-1:0] valid;
    example_pkg::control_t [example_pkg::capacity-1:0] control;
    logic [example_pkg::capacity-1:0][$bits(example_pkg::control_t)-1:0] control_mask;


    // Shorter groups still align their dimensions and names.
    logic  [COUNT-1:0] valid_small;
    word_t [COUNT-1:0] data_small;
endmodule
