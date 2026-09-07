// Spacing before a recovered assignment is stable when the right side wraps.
module foo;
    `DECLARE(logic, result) =
        `VALUE(signal_with_a_long_name, package_name::type_with_a_long_name, another_signal_with_a_long_name);
endmodule
