// Conditional directives inside a comma-separated enum value list follow
// the same dedent rule as port lists: the `ifdef`/`endif` sits one level
// above the value column while the values stay at the normal column,
// rather than indenting the conditional body +1.
package pkg;
    typedef enum int {
        STATE_ALPHA,
        STATE_BRAVO,
        STATE_CHARLIE,
    `ifdef FEATURE_EXTRA
        STATE_DELTA,
    `endif
        STATE_LAST
    } state_e;
endpackage
