// A macro-provided hierarchy prefix remains joined to the member call in a loop.
module foo;
    initial begin
        for (int i = 0; i < 4; i++) `TARGET.write_value(i);
    end
endmodule
