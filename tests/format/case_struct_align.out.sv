module case_align;
    logic [2:0] sel;
    logic [7:0] result;

    always_comb begin
        case (sel)
            3'b000:
                result = 8'h00;
            3'b001:
                result = 8'h11;
            3'b010:
                result = 8'h22;
            3'b100,
            3'b101:
                result = 8'hFF;
            default:
                result = 8'hXX;
        endcase
    end

    // Struct assignment pattern
    typedef struct packed {
        logic [7:0] addr;
        logic [3:0] len;
        logic       valid;
    } request_t;

    request_t req;
    assign req = '{
        addr:  8'hAB,
        len:   4'd3,
        valid: 1'b1
    };
endmodule
