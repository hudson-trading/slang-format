// A comma following a macro argument belongs to the same list item and must
// not acquire a different continuation indent on the next formatting pass.
`define READ(VALUE) VALUE

module example;
    function void update();
        result = calculate_updated_value(
                     current_field,
                     `READ(current_register),
                     replacement_value
                 );
    endfunction
endmodule
