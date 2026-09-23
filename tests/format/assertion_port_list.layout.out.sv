// let, property, sequence, and checker parameters stay inline when they fit.
// Long or commented lists wrap one parameter per line.
module foo;
    let enabled = 1'b1;
    let constant_value() = 1'b1;
    let identity(value) = value;
    let choose_lower(a, b) = (a < b ? a : b);
    let add(a, b) = a + b;


    let offset(int value, int amount = 1) = value + amount;

    let long_parameters(
        first_input_with_a_long_name,
        second_input_with_a_long_name,
        third_input_with_a_long_name
    ) = 0;

    let annotated(
        a,  // First input.
        b
    ) = a + b;

    property follows(a, b);
        a |-> b;
    endproperty

    property long_form(
        first_input_with_a_long_name,
        second_input_with_a_long_name,
        third_input_with_a_long_name
    );
        first_input_with_a_long_name |-> second_input_with_a_long_name;
    endproperty

    sequence consecutive(a, b);
        a ## 1 b;
    endsequence

    sequence annotated_sequence(
        a,  // First input.
        b
    );
        a ## 1 b;
    endsequence
endmodule

checker monitor(input logic clk, input logic reset);
endchecker

checker wide_monitor(
    input logic [63:0] first_signal_with_long_name,
    input logic [63:0] second_signal_with_long_name
);
endchecker
