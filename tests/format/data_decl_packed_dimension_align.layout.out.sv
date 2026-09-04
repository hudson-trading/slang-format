// Mixed base types with matching packed dimensions align both the dimension
// and declarator columns instead of putting all padding before the name.
module data_decl_packed_dimension_align;
    logic [lane_count-1:0] valid_by_stage[stage_count:0];
    datapath_pkg::word_t [lane_count-1:0] data_by_stage[stage_count:0];
    logic [lane_count-1:0] ready_by_stage[stage_count:0];
    datapath_pkg::metadata_t [lane_count-1:0] metadata_by_stage[stage_count:0];
endmodule
