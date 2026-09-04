// This tests interaction of ifdefs and comments
`ifdef PLATFORM_FPGA
    // FPGA-specific implementation
    module platform_mod (
        input  logic clk,
        output logic out
    );
        // Use BRAM inference
        /* Note: Vivado will infer BRAM for arrays >= 4096 bits */
        logic [31:0] mem [128];

        `ifdef XILINX
            // Xilinx-specific attribute
            (* ram_style = "block" *)
            logic [31:0] bram [256];
        `elsif ALTERA // Intel/Altera path
            /* Altera uses a different attribute */
            logic [31:0] bram [256] /* synthesis ramstyle = "M20K" */;
        `endif

        assign out = |mem[0];
    endmodule
`else  // not FPGA
    // ASIC implementation
    module platform_mod (
        input  logic clk,
        output logic out
    );
        /* Standard ASIC memory - no special attributes needed */
        logic [31:0] mem[128];
        assign out = |mem[0];
    endmodule
`endif  // PLATFORM_FPGA
