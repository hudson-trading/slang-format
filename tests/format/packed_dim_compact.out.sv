// Inside packed-dim `[...]` brackets, binary operators stay compact (no spaces)
// even when nested inside function/system-call arguments. Outside brackets,
// spaces are normalized as usual.
//
// Rule (recursive): once you're inside `[...]`, you stay compact for everything
// nested — `[$clog2(N+1)-1:0]`, `[W*2+1:0]`, `arr[i+1]`. Top-level RHS
// expressions still get normal spacing. The one-line standalone annotations
// and single blank lines do not split the two typedefs' alignment group.

package pkg;
    parameter int N = 64;
    parameter int W = 8;

    // System-call args inside packed-dim: stay compact.
    typedef logic [$clog2(N+1)-1:0] count_t;

    // Nested binary op inside packed-dim: stay compact.
    typedef logic [W*2+1:0]         dword_t;

    // Index expression with binary op: stay compact.
    function automatic logic [7:0] get_byte(logic [127:0] data, int i);
        return data[i+1];
    endfunction

    // Top-level binary op outside any bracket: normal spaces.
    parameter int M = N + 1;

    // Function call outside packed-dim: arguments still get spaces.
    parameter int K = $clog2(N + 1);
endpackage
