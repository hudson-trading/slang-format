module struct_union;
    typedef struct packed {
        logic [7:0] opcode;
        logic [7:0] rs1;
        logic [7:0] rs2;
        logic [7:0] rd;
    } instr_t;

    typedef union packed {
        instr_t decoded;
        logic [31:0] raw;
    } instr_word_t;

    typedef struct packed {
        logic valid;
        logic [2:0] tag;
        union packed {
            logic [15:0] immediate;
            struct packed {
                logic [7:0] hi;
                logic [7:0] lo;
            } split;
        } operand;
    } complex_t;

    instr_word_t word;
    complex_t cplx;

    initial begin
        word.raw = 32'hDEADBEEF;
        cplx.valid = 1'b1;
        cplx.tag = 3'b010;
        cplx.operand.split.hi = 8'hAB;
        cplx.operand.split.lo = 8'hCD;
    end
endmodule
