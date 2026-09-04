// A macro-provided enum segment must not take ownership of the next item's comment.
package example_pkg;
    typedef enum {
        first,
        `INCLUDE_ITEMS("items.svh")
        last // final item
    } item_t;
endpackage
