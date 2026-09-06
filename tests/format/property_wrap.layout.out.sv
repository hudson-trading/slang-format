// Concurrent assertion (`assert`/`assume`/`cover property`) wrapping.
//
// - Short body fits inline on the header line.
// - Long body: break `(`/`)` to their own lines so the structure reads
//   top-down, and wrap the body at the lowest-precedence property
//   operator (here `|->`).
// - When nested inside a wrapping `if` (so the `assert` keyword sits on
//   its own indented line), the short form still inlines — the special
//   formatter must flush its own buffer so the body tokens don't leak
//   into a later context and end up unindented.

module dut;
    short_assume: assume property (a |-> b);

    nested_in_if: always @(posedge clk)
        if (~rst)
            assert property (~a_signal);

    long_wrap: assume property (
        (!rst && some_module_inst.req_valid) |->
        (some_module_inst.req_addr[ADDR_WIDTH-1:CLIENT_ADDR_WIDTH] == BASE_ADDR)
    );
endmodule
