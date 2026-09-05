// A Dynamic list with only one member never benefits from going vertical
// — wrapping it just adds two lines of brace noise without separating
// anything. Verticalize only when there are 2+ members to actually
// separate.
//
// Most visible in `inside { ONE_VAL }` inside a deeply-nested struct
// assignment pattern: even when the inline form blows past the column
// limit, the single-member set stays inline; only the outer expression
// (the ternary) wraps.

module dut;
    initial begin
        compute_op_sink[port] = compute_op_combined_t'{
            matmul: resv[tid_token][port].meta.unit_opcode == OP_MUL,
            set: resv[tid_token][port].meta.unit_opcode == OP_SET,
            emit: resv[tid_token][port].meta.unit_opcode == OP_EMIT,
            // Multi-member: goes vertical when it doesn't fit.
                accum_half_sel: resv[tid_token][port].meta.unit_opcode inside {OP_SET, OP_EMIT}
                ? resv[tid_token][port].meta.accum_half_sel
                : pkg::half_select_t'(0),
            // Single member: stays inline even when overall expression wraps.
                fused_leakyrelu: resv[tid_token][port].meta.unit_opcode inside {OP_EMIT}
                ? resv[tid_token][port].meta.fused_relu_select
                : pkg::fused_leakyrelu_t'(0)
        };
    end
endmodule
