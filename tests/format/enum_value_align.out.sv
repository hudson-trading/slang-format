// Enum value members (bare declarators in a comma-separated list) align at
// the `=` like ports and parameters do. Members with trailing comments align
// the comment column too; members without an initializer leave the value
// column empty without breaking the group.
package pkg;
    localparam int WIDTH = 8;
    typedef enum logic [WIDTH-1:0] {
        OP_UNSET    = 'd0,  // first comment
        OP_NOP      = 'd1,  // second comment
        OP_EQ       = 'd2,
        OP_LONGNAME = 'd3,
        OP_X        = 'd10
    } op_t;

    // Contrast: an unvalued enum list needs no `=` column, so nothing is
    // padded.
    typedef enum {
        RED,
        GREEN,
        BLUE
    } color_t;

    // Mixed: some members valued, some not. The valued rows align; the bare
    // ones leave the value column empty.
    typedef enum int {
        A   = 1,
        BB,
        CCC = 3
    } mixed_t;
endpackage
