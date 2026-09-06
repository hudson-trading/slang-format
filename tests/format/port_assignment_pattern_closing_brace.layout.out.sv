// A port assignment pattern's closing brace returns to the pattern type's indentation.
module assignment_pattern_port;
    consumer
      consumer_i (
        .payload(record_t'{
                     first_field: first_value,
                     second_field: second_value,
                     third_field: source.third_value
                 }),
        .valid(valid),
        .ready(ready)
    );
endmodule
