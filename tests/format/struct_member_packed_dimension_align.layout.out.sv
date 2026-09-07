// Struct members align packed dimensions across built-in, named, and scoped types.
module struct_member_packed_dimension_align;
    typedef logic [7:0] lane_t;

    typedef struct packed {
        types_pkg::flags_t [lane_count-1:0] flags;
        logic [lane_count-1:0] signs;
        lane_t [lane_count-1:0] values;
    } payload_t;
endmodule
