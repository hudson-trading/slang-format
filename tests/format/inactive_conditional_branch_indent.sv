// Inactive conditional bodies gain one indentation level, while conditional
// entries in separated lists retain the list's normal item indentation.
module foo;
`ifdef FEATURE
logic signal_a;
`else
logic signal_b;
`endif
endmodule

package pkg;
typedef enum int {
VALUE_A,
`ifdef FEATURE
VALUE_B,
`endif
VALUE_C
} value_t;
endpackage
