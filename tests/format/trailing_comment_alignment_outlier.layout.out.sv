// A long commented row does not push shorter trailing comments far to the right.
module trailing_comment_alignment_outlier #(
    parameter int index_width = $clog2(storage_depth),  // bounded index width
    parameter int chunk_size = 2048,  // bytes per chunk
    parameter int lane_count = 3  // parallel lanes
) ();
endmodule
