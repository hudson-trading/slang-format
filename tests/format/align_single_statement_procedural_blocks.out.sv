// A singleton procedural section does not inherit a neighboring group's alignment.
module singleton_section;
    always_comb first        = a;
    // A comment without a blank line does not split the group.
    always_comb longer_first = b;

    // A section header after a blank line starts a new group.
    always_comb x = c;
endmodule

module multirow_sections;
    always_comb longest_shared_name = f;
    always_comb shared_a            = g;

    // Multi-row sections keep sharing alignment across a soft boundary.
    always_comb shared_b            = h;
    always_comb shared_c            = i;
endmodule
