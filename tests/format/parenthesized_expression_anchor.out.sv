// Long assertions open vertically; inner breaks cannot outdent past their parentheses.
module test;
    CHECK_RESPONSE: assert property (
        disable iff(reset)
        ((request_active && request_valid && !response_valid) |=> (s_eventually response_valid))
    );

    always_comb begin
        source = ready && ((item.long_identifier >= identifier)
                               || item.another_long_identifier) ? CURRENT : next_source;
    end
endmodule
