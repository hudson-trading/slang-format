// Alignment is computed from the final monotonic set of layout breaks, so a
// second formatting run cannot discover new alignment opportunities.
interface foo
import pkg::*;
#(
parameter type request_t = logic,
parameter type longer_response_t = logic
);
endinterface

module bar #(
parameter int short_name = some_package::a_value,
parameter int much_longer_name = some_package::a_function_with_a_long_name(argument_a, argument_b)
) ();
endmodule
