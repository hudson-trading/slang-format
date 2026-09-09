// Block-comment spacing is stable when list delimiters move onto new lines.
interface bus #(
    parameter int width /* public */ = 8
) (
    input logic clk   /* public */,
    input logic reset /* public */
);
    typedef enum {
        FIRST  /* first */,
        SECOND /* second */
    } state_t;
endinterface
