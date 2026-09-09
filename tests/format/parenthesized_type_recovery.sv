// Unsupported parenthesized types must survive recovery across declarations.
module foo;
    typedef logic [3:0] data_t;
    typedef (data_t) alias_t;
    (* keep *) (alias_t) value = 4'b0010;
endmodule
