// A macro-expanded with clause that starts on its own line must retain the
// covergroup member indentation on subsequent formatting passes.
`define FILTER with

covergroup example_group;
    point_a: coverpoint value_a;
    point_b: coverpoint value_b;

    cross_ab: cross point_a, point_b {
        ignore_bins disabled = cross_ab
        `FILTER (!enabled);
    }
endgroup
