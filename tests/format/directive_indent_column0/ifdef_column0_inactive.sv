module column0_inactive;
    `ifdef NEVER_DEFINED
        `ifdef ALSO_NEVER
            logic a;
        `else
            logic b;
        `endif
        assign x = 1;
    `endif

    initial begin
        `ifndef SYNTHESIS
            $display("sim");
        `else `ifdef FPGA
            $display("fpga");
        `endif
        `endif
    end

    function automatic int f(int i);
        `ifdef UNSET
            return i + 1;
        `endif
        return i;
    endfunction
endmodule
