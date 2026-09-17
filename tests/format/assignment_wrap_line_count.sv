// Move a binary RHS below the assignment only when doing so saves lines.
module demo;
    wire some_intentionally_long_result_name_for_wrapping = signal_a && signal_b && signal_c && signal_d && signal_e && signal_f && signal_g && signal_h && signal_i && signal_j && signal_k && signal_l;

    // Equal line counts keep the first operand beside the assignment.
    wire another_result_name = signal_a && signal_b && signal_c && signal_d && signal_e && signal_f && signal_g;

    // Short expressions remain on one line.
    wire short_result = signal_a && signal_b;

    always_comb begin
        some_long_result_name = signal_a + signal_b + signal_c + signal_d + signal_e + signal_f + signal_g + signal_h + signal_i + signal_j;
        another_result_name += signal_a + signal_b + signal_c + signal_d + signal_e + signal_f;
        some_intentionally_long_result_name_for_wrapping = signal_a && signal_b && signal_c && signal_d && signal_e && signal_f && signal_g && signal_h && signal_i && signal_j && signal_k && signal_l;
    end

    always_ff @(posedge clk) begin
        some_intentionally_long_result_name_for_wrapping <= signal_a && signal_b && signal_c && signal_d && signal_e && signal_f && signal_g && signal_h && signal_i && signal_j && signal_k && signal_l;
        another_result_name <= signal_a && signal_b && signal_c && signal_d && signal_e && signal_f && signal_g;
    end
endmodule
