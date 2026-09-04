// A macro argument in a packed-dimension function call must reach its canonical
// spacing and continuation indentation in one formatting pass.
package example_pkg;
typedef struct packed {
logic [maximum(1, `ITEM_COUNT)-1:0][7:0] items;
logic [127:0] tag;
} record_t;
endpackage
