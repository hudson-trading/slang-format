// Enum member declarators may carry a dimension (e.g. STATE_A[4] expands to
// STATE_A0..STATE_A3). The bracket attaches to the declarator name and must
// not gain a leading space, even though the enum is a DataTypeSyntax.
package state_pkg;
    typedef enum logic [3:0] {
        STATE_A[4],
        STATE_B[5],
        STATE_DONE
    } state_t;

    // Contrast: regular variable declarator with an unpacked dim.
    logic [7:0] foo[4];

    // Contrast: type-level dimension (space before the bracket is intentional).
    logic [7:0] bar;
endpackage
