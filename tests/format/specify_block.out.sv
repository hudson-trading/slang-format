module specify_test (
    input  logic a, b, sel,
    output logic y
);
    assign y = sel ? a : b;

    specify
        specparam trise = 1.5;
        specparam tfall = 2.0;
        (a => y) = (trise, tfall);
        (b => y) = (trise, tfall);
        (sel *> y) = (1.0, 1.2);
    endspecify
endmodule
