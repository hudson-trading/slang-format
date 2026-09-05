module mixed_ports (
    iface_a.sink port_a1,
    iface_b_wide.sink port_a2,
    input logic port_a3,

    output logic port_b1,
    iface_a.sink port_b2,

    iface_c.source port_c1,
    output logic port_c2,

    iface_c.source port_c3,
    output logic port_c4,

    iface_c.source port_d1,

    input pkg_x::wide_t port_e1,

    iface_d.slave port_f1
);
endmodule

module pure_iface (
    iface_a.sink p_one,
    iface_c.source p_two,
    iface_d.slave p_three
);
endmodule

module pure_var (
    input logic p_foo,
    output logic p_bar
);
endmodule

typedef struct packed {
    field_a_type_t field_a;
    field_b_t field_b;
    logic field_c;
    logic field_d;
    field_e_t field_e;
} my_struct_t;
