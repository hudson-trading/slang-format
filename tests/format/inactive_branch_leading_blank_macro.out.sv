// Inactive branch reparsing must not accumulate blank lines before a macro.
module foo;
    `ifdef FEATURE

        `CHECK
        logic value;
    `endif
endmodule
