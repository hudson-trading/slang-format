// A ternary table keeps its first row after the assignment when that row fits;
// later predicates align with the first predicate instead of forcing a break at `=`.
module ternary_chain_keeps_assignment_line;
    always_comb begin
        header_next = (header_count >= 'd2) ? header_fifo[second_slot] : second_from_low ? event_header_even : event_header_odd;

        // The assignment still breaks when the first table row does not fit.
        long_header_next = (long_header_count >= 'd2) ? long_header_fifo[long_second_slot].event_header_data : long_second_from_low ? long_event_header_even : long_event_header_odd;
    end
endmodule
