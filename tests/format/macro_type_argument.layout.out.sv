// A macro type must remain separated from the argument name.
function void foo(
    `TYPE value,
    int index = -1
);
endfunction

// Macros inside packed dimensions still belong to the enclosing type.
typedef logic [$clog2(`COUNT)-1:0] index_t;
