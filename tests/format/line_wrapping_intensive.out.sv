module line_wrapping_intensive;

    // Long lines with nested expressions
    always_comb aaaaaaaaaaa[BBBBBBBBBBBBBBBBBBBBBB] =
        ((aaaaaaaaa::BBBBBBBB(ccccccccccccccccc[1]) || aaaaaaaaa::CCCCCCCC(ccccccccccccccccc[1]))
             && dddddddddddddddddd && ((eeeeeeeeeeeeee & (1 << ffffffffffff.gggg.hhhhhh)) == 0))
            || (aaaaaaaaa::DDDDDDD(ccccccccccccccccc[1])
                    && ((eeeeeeeeeeeeee & (1 << ccccccccccccccccc[1].iiii.jjjjjj.kkkk)) == 0))
            || (aaaaaaaaa::isDDDDDDD(ccccccccccccccccc[1])
                    && ((eeeeeeeeeeeeee & (1 << ccccccccccccccccc[1].lllllll.mmmmmmmmm)) == 0));

    // Module port wrap with ternary
    SomeModule #(
        .aaaaaaaa (bbbbbbbbbbbbbbbbbbbbbb && cccccccccccccccccccc
                       && dddddddddddddddddddddddddddddddddddd == eeee ? 2'(ffffffffffff) : '0)
    ) i_SomeModule (
        .a    (),
        .port (bbbbbbbbbbbbbbbbbbbbbb && cccccccccccccccccccc
                   && dddddddddddddddddddddddddddddddddddd == eeee ? 2'(ffffffffffff) : '0),
        .c    ()
    );

    always_ff @(posedge clk) begin
        if (rst) begin end else begin
            if (a) begin
                if (b) begin end else if (aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
                                              || (bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb == cccccccccccccccccccccccc)
                                              || dddddddddddddddddddddddddddddd)
                begin end
            end
        end
    end

    // Dec with newline
    wire a =
        bbbbbbbbbb && ccccccccccccccccccccccc && (dddddddddddddddddddddd || eeeeeeeeeeeeeeeeeeeee);

    // Dec with more newlines
    wire a =
        bbbbbbbbbb && ccccccccccccccccccccccc && (dddddddddddddddddddddd || eeeeeeeeeeeeeeeeeeeee);

    // Long expression
    always_ff @(posedge clk) begin
        aaaaaaaaaaaaaa <=
            bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb
                && (((cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc == ddddddddddddddddddddddddd)
                         && (eeeeeeeeeeeeeeeeee(fffffffffffffffffffffffffffffffff)))
                        || ((gggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg == hhhhhhhhhhhhhhhhhhhhhhhhh)
                                && (iiiiiiiiiiiiiiiiii(jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj)))
                        || ((kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk == llllllllllllllllllllllllllllllllllllllll)
                                && (mmmmmmmmmmmmmmmmmm(nnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnn))));
    end

    // 100 character variable name
    always_comb begin
        aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa =
            bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb
                * cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc
                * dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd;
    end

    // Ternary with expression line wrap
    always_comb aaaaaaaaaaaaaaaa =
        bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb
            + bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb
            ? cccccccccccccccccccccccccccccccccccc
                 + ddddddddddddddddddddddddddddddddddddddddddddddddddddddddd
            : eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee
                 + fffffffffffffffffffffffffffffffffffffffffffffffffff;

    // Line wrapping within ElementSelect
    always_ff @(posedge clk) begin
        if (a) begin
            if (b) begin
                if (c) begin
                    if (d) begin
                        if (e) begin
                            if (f) begin
                                if (g) begin
                                    if (h) begin
                                        if (((aaaaaaaaaaaaa
                                                  >> bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb)
                                                 + (ccccccccccccc[$clog2(dddddddddddddddddddddddddddddddddd)-1:0] != '0)))
                                        begin end
                                    end
                                end
                            end
                        end
                    end
                end
            end
        end
    end

endmodule
