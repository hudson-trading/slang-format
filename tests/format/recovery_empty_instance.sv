// Idempotency: slang's recovery sometimes synthesizes a HierarchicalInstance
// with no real tokens (no name, missing parens) — typical for partial-parse
// .vh snippets that start with `,.name (...)`. The contCol "newline + indent"
// emitted before each instance would leave trailing whitespace if the
// synthesized instance emits nothing, which then re-parses as leading trivia
// on the next member and compounds another indent on the next pass. Skipping
// the contCol injection for those phantom instances keeps the output stable.
,.port_a (
    {
        wide_bus[71:64],
        wide_bus[63:56],
        wide_bus[55:48],
        wide_bus[47:40],
        wide_bus[39:32],
        wide_bus[31:24],
        wide_bus[23:16],
        wide_bus[15:8],
        wide_bus[7:0],
        pad[0],
        pad[1],
        pad[2]
    }
)
,.port_b (
    {
        pad[3]
    }
)
