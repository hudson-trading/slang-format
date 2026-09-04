// A long single ternary wraps `?` and `:` together, with both branches
// indented from the predicate instead of leaving an overlong first branch.
module single_ternary_consistent_wrap;
    always_comb begin
        if (request_valid) begin
            response.status =
                current_status == status_pkg::STATUS_ACCEPTED
                    ? status_pkg::STATUS_RETRY_RECOMMENDED
                    : current_status;
        end

        // Contrast: a short single ternary stays inline.
        short_result = select_a ? value_a : value_b;
    end
endmodule
