module macro_verbatim_trivia;
    typedef logic [7:0] byte_t;

    typedef struct packed {
        byte_t          plain;              //  1
        `macro_bytes(4) macro_field;        //  4 -> 5
    } macro_struct_t;

    initial begin
        `uvm_info("TRACE", $sformatf("    wr %0d  comp=%0d", idx, comp), UVM_DEBUG)

        payload = '{
            `HTOL4(32'h00_00_00_02),                            // NumVectors (2)
            //`HTOL4(32'h03_05_11_77),                            // MacID mask
            `HTOL4(32'h00_00_00_01)                             // MacID mask
        };
    end
endmodule
