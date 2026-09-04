// A block comment before a binary operator remains expression trivia when the
// expression wraps immediately after the comment.
module example;
    initial begin
        expected_count += 1 /*first term*/ + observed_transaction.data_count /*second term*/
                              + (observed_transaction.stopped ? 1 /*last term*/ : 0);
    end
endmodule
