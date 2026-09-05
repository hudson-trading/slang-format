// Blank lines between macros and surrounding members should round-trip
// idempotently. Macro directives in SV consume their terminating EOL,
// so the blank-line counter must be pre-biased when the previous entry
// was such a macro.
module test;
    `MY_MACRO(a)

    // section header
    logic [7:0] foo;

    `OTHER_MACRO(b)


    // two blank lines above this
    logic [7:0] bar;

    (* `ATTR *) `MY_MACRO(c)

    // attribute+macro pattern
    logic [7:0] baz;

    initial begin
        `LOG_DEBUG("hi");

        x = 1;
    end
endmodule
