// Off/on regions preserve list items and cannot close across nested list scopes.
module foo(input logic clk, input logic enable);
    logic a,b,c;
    // slang-format: off
    localparam int LEFT=1;localparam int RIGHT= 2;
    // slang-format: on
    localparam int normal=3;
    initial begin
        // slang-format: off
        a=1;b =2;
        // slang-format: on
        c=3;
        begin
            // slang-format: off
            a=4;b=5;
        end
        // slang-format: on
        c=6;
    end
    // slang-format: off
    always @(posedge clk) begin
        // slang-format: on
        if(enable) a=1;
    end
    assign b=enable;
    // slang-format: on
    assign c=enable;
endmodule

// slang-format: off
module bar;logic a;endmodule
module baz;logic b;endmodule
// slang-format: on
module qux;logic c;endmodule
