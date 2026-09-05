module case_statements (
    input logic [3:0] opcode,
    input logic [7:0] a, b,
    output logic [7:0] result
);
    // Basic case
    always_comb begin
        case (opcode)
            4'b0000: result = a + b;
            4'b0001: result = a - b;
            4'b0010: result = a & b;
            4'b0011: result = a | b;
            4'b0100: result = a ^ b;
            default: result = 8'h00;
        endcase
    end

    // Unique case
    logic [7:0] priority_result;
    always_comb begin
        unique case (opcode[1:0])
            2'b00: priority_result = a;
            2'b01: priority_result = b;
            2'b10: priority_result = a + b;
            2'b11: priority_result = a - b;
        endcase
    end

    // Casez with wildcard
    logic hit;
    always_comb begin
        casez (opcode)
            4'b1???: hit = 1'b1;
            4'b01??: hit = 1'b1;
            default: hit = 1'b0;
        endcase
    end

    // Case inside
    logic match;
    always_comb begin
        case (opcode) inside
            [0:3]: match = 1'b1;
            [8:15]: match = 1'b1;
            default: match = 1'b0;
        endcase
    end
endmodule
