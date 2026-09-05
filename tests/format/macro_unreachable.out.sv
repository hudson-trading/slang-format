module macro_unreachable (
    input  logic [1:0] sel,
    output logic       out
);
    always_comb begin
        case (sel)
            2'b00:   out = 1'b0;
            2'b01:   out = 1'b1;
            default: `UNREACHABLE
        endcase
    end
endmodule
