// Recovery must not interpret an equality operator as an assignment.
module foo;
`DECLARE(logic, result) = signal_a == signal_b && signal_c;
`DECLARE(pointer_t, next_ptr) =
    // Equality in the recovered RHS is not an assignment.
    state.ptr == state.last ?
        state.base :
        state.ptr + pointer_t'(1);
endmodule
