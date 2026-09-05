// Test: consecutive `typedef <type> <name>;` declarations align their
// type column so the new-type names line up. Typedefs only align with
// other typedefs. One-line comments and single blank lines do not split the
// group; an incompatible member, two blank lines, or a longer comment region
// can. Struct/enum-bodied and dimensioned typedefs emit normally.

package p;
    typedef ExtPkg::uint8_t  uint8_t;
    typedef ExtPkg::int8_t   int8_t;
    typedef ExtPkg::uint16_t uint16_t;
    typedef ExtPkg::int16_t  int16_t;
    typedef ExtPkg::uint32_t uint32_t;
    typedef ExtPkg::int32_t  int32_t;
    typedef ExtPkg::uint64_t uint64_t;
    typedef ExtPkg::int64_t  int64_t;

    // The one-line comment and single blank above do not split the group.
    typedef logic [7:0]      byte_t;
    typedef logic [15:0]     half_t;
    typedef logic [31:0]     word_t;

    // A non-typedef member between typedefs splits the group.
    typedef int              first_t;
    logic some_signal;
    typedef int second_t;

    // Bodied / dimensioned typedefs don't align (emit normally).
    typedef struct packed {
        logic a;
        logic b;
    } pair_t;
    typedef logic queue_t[3:0];
endpackage
