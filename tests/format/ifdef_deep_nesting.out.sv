module deep_ifdef (
    input  logic       clk,
    output logic [7:0] out
);
    `ifdef PLATFORM_A
        `ifdef VARIANT_X
            `ifdef FEATURE_1
                assign out = 8'hAA;
            `else
                assign out = 8'hBB;
            `endif
        `elsif VARIANT_Y
            assign out = 8'hCC;
        `else
            assign out = 8'hDD;
        `endif
    `elsif PLATFORM_B
        `ifdef VARIANT_X
            assign out = 8'hEE;
        `else
            assign out = 8'hFF;
        `endif
    `else
        assign out = 8'h00;
    `endif
endmodule
