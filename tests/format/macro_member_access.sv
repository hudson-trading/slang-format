`define MEM_p u_unit.mem_bank

module top;
    function void mem_wr(int idx, logic [31:0] data);
        `MEM_p[idx] = data;
    endfunction

    function logic [31:0] mem_rd(int idx);
        return `MEM_p[idx];
    endfunction
endmodule
