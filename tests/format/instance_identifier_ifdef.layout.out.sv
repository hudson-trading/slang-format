module instance_identifier_ifdef;
    Sender
    `ifdef AGGRESSIVE
        #(.PERIOD(4))
    `endif
    sender (.clk(clk), .rst(rst));

    `ifdef USE_ALT
        AltReceiver
    `else
        DefaultReceiver
    `endif
    receiver (.clk(clk), .rst(rst));

    `ifdef USE_ALT
        AltLeaf
    `else
        DefaultLeaf
    `endif
    leaf (.*);
endmodule
