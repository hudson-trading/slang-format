// A lone initialized enum member has no other `=` to align with, so a longer
// bare member must not add padding before that one initializer.
typedef enum int {
    FIRST = 0,
    A_MUCH_LONGER_BARE_MEMBER,
    LAST_BARE_MEMBER
} single_initializer_t;

// Contrast: with two initialized members, their equals signs still align.
typedef enum int {
    FIRST = 0,
    A_MUCH_LONGER_BARE_MEMBER,
    LAST_INITIALIZED = 2
} multiple_initializers_t;
