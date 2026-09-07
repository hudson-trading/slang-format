// A single ternary on an aligned assignment keeps its predicate and true
// branch inline, then starts the wrapped false branch with `:` aligned to `?`.
module single_ternary_false_branch_align;
    always_comb begin
        packet_header.destination = source_destination;
        packet_header.op = (|packed_data[31:24]) ? operation_pkg::OP_BUFFER_CLEAR : operation_pkg::OP_COMPUTE;
    end
endmodule
