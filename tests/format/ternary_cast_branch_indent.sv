// Each ternary branch inside a wrapped cast anchors its own binary continuation.
module demo;
    always_comb begin
        result = foo_pkg::value_t'(select_value ? bar_pkg::first_long_constant + bar_pkg::second_long_constant : bar_pkg::third_long_constant + some_record.some_field);

        // Parenthesized branches retain their own expression anchors.
        grouped_result = foo_pkg::value_t'(select_value ? (bar_pkg::first_long_constant + bar_pkg::second_long_constant) : (bar_pkg::third_long_constant + some_record.some_field));

        // A plain binary cast operand keeps its existing continuation indentation.
        plain_result = foo_pkg::value_t'(bar_pkg::first_long_constant + bar_pkg::second_long_constant + bar_pkg::third_long_constant);

        // Short branches stay inline.
        short_result = foo_pkg::value_t'(select_value ? a + b : c + d);
    end
endmodule

// The branch anchors also survive a break before the cast type.
package example;
    localparam foo_pkg::value_t some_intentionally_long_result_name_for_wrapping = foo_pkg::value_t'(select_value ? bar_pkg::first_long_constant + bar_pkg::second_long_constant : bar_pkg::third_long_constant + some_record.some_field);
endpackage
