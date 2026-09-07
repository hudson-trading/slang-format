// A closing conditional comment must not absorb the following comment block.
`ifdef FEATURE
`endif  // FEATURE

// Define a helper.
//
// The body is intentionally multiline.
`ifndef HELPER
    `define HELPER(VALUE) \
    VALUE = 1; \
    VALUE += 2;
`endif
