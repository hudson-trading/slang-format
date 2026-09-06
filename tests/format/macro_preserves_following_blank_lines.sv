// Multiple blank lines after a macro remain intact in an inactive conditional branch.
module macro_preserves_following_blank_lines;
`ifdef OPTIONAL_CHECKS
    initial begin
        `CHECK_READY(signal_a)


        // A separate group follows the macro.
        signal_b = signal_a;
    end
`endif
endmodule
