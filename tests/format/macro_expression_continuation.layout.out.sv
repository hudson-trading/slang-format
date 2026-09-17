// An operator after a macro-expanded operand remains part of the same binary
// expression even when formatting places it on the next line.
`define WIDTH 32
`define ENABLED enabled

module example;
    function void calculate();
        accumulated_symbol_score =
            (32'(unscaled_symbol_score) * `WIDTH
                > 32'hffff) ? 32'hffff : 32'(unscaled_symbol_score) * `WIDTH;
        command_is_internal = command_opcode == READ_VALUE && `ENABLED
                              && is_recognized_internal_command(command_opcode);
    endfunction
endmodule
