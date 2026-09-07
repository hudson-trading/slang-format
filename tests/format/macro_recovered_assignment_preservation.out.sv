// Recovered assignments keep macro arguments opaque even beyond the column limit.
module foo;
    `DECLARE(package_name::result_type, output_signal) =
        `VALUE(input_signal_with_a_long_name, package_name::result_type_with_a_long_name, input_signal_width_with_a_long_name);
    `DECLARE(logic, text_signal) = `STRINGIFY(first  &&  second);
endmodule
