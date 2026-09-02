# Formatting Philosophy

`slang-format` is an **opinionated** formatter — configuration is kept to a minimum, and formatting decisions are driven almost entirely by the concrete syntax tree (CST) rather than existing whitespace or user style. The goal is consistent, readable SystemVerilog with zero bikeshedding.

Some things are preserved from the original source:

- **Macro definitions and Macro args** — these are emitted verbatim since their body is not parsed into a syntax tree.
- **Blank lines within lists** — blank lines between module members, port declarations, etc. are kept as authored to respect logical groupings.
- **Expressions with existing line breaks** — when `respectUserFormatting` is enabled, an existing layout is retained as one all-or-nothing choice if it fits the column limit.

## Formatter Passes

Formatting is split into three conceptual passes:

1. **Normalize** wraps source tokens in formatter-owned nodes, attaches same-line comments to the token they trail, separates standalone comments and blank lines, and materializes nested preprocessor conditionals as synthetic branch nodes. Inactive branches are reparsed and formatted recursively only when the complete token stream survives; an unsafe branch alone falls back to opaque, re-indented text.
2. **Layout** applies token spacing and solves member-local soft-line choices. This output is available with `--stage layout` and is independently validated and idempotent.
3. **Align** groups compatible first-line anchors and adds padding. Alignment is split-only relative to layout: it may retain or add line breaks, but cannot join a member that the layout pass split.

## Lists

Lists are either always vertical, always inline (like array dimensions), or dynamic based on heuristics like children count or length.

## Ifdef Indentation

In member lists, preprocessor conditional directives (`ifdef`/`ifndef`/`else`/`elsif`/`endif`) are indented as structural blocks, not left-flushed to column 0. The content between an `ifdef` and its matching `else`/`endif` is indented one level deeper:

```systemverilog
module top;
    `ifdef FEATURE_A
        logic a;
        `ifdef ASSIGN
            assign a = 1;
        `endif
    `else
        logic b;
        assign b = 0;
    `endif
endmodule
```

This is because these behave nearly identically to generate blocks, which follow normal indenting.

In non-member lists like port lists or case items, ifdefs are dedented one level so that the list items across branches stay aligned. In these contexts, ifdefs tend to act as feature flags toggling individual entries rather than introducing structural blocks:

```systemverilog
module top (
    input logic clk,
    input logic rst,
`ifdef HAS_DEBUG
    input logic debug_en,
`endif
    output logic valid
);
```

## Line Wrapping

Long binary, property, and sequence-expression chains are lowered to hierarchical formatter IR and solved with dynamic programming. Every soft line receives a priority from its normalized IR depth: outer boundaries are admitted before boundaries close to leaf expressions. Same-precedence binary chains share one tier.

### Algorithm

1. Normalize the complete clean member into tokens, atomic text, hard lines, and prioritized soft lines.
2. Try successively deeper priority tiers. The first tier that can satisfy the column limit wins.
3. Within a tier, minimize worst overflow, total overflow, number of breaks, and raggedness, in that order.
4. If no layout can fit because an atomic token is itself too wide, minimize worst overflow and still split around that token rather than leaving one giant line.

Dynamic lists are represented as consistent soft-line groups, so all comma boundaries split together; the solver never bin-packs only part of a list. Lists and specialized ternary layouts continue through their syntax-specific renderers while their alignment anchors are migrated to the shared IR.

### Examples

Binary expressions break at the lowest-precedence operator:
```systemverilog
// Before wrapping:
assign result = a + b * c + d * e + f;

// After wrapping (breaks at +, not at *):
assign result =
    a + b * c + d * e
    + f;
```

Ternary expressions break before `?` and `:`:
```systemverilog
// Before:
assign out = (sel == 2'b00) ? input_a : (sel == 2'b01) ? input_b : input_c;

// After:
assign out = (sel == 2'b00)
    ? input_a
    : (sel == 2'b01)
        ? input_b
        : input_c;
```

### Continuation Indentation

Operator continuations use one stable continuation indent for the member. Expression depth affects break priority, not indentation, so nested CST wrappers do not create staircase indentation. First-line alignment anchors likewise do not push continuation lines farther right.

### What Doesn't Wrap

- **Data declarations** are not split — they stay on one line regardless of length.
- **Concatenations** (`{a, b, c}`) and **function arguments** are handled by Dynamic list verticalization, not expression wrapping. When an overflowing function call is the sole assignment RHS, the whole call moves to the next indented line; short calls remain inline.
- **Macro invocations** are emitted as raw text and are not individually wrapped.

## Alignment

The formatter supports column-aligned declarations within groups of consecutive members of the same kind. Columns are padded so that identifiers, types, and dimensions line up vertically:

```systemverilog
input  logic [7:0]  data_in,
input  logic        valid,
output logic [15:0] data_out
```

### Alignment Model

Each alignable row produces a tree of `AlignCell`s derived from its syntax. Consecutive compatible rows form a group, and the framework computes per-column max widths and emits each row with padding between columns. When sub-tree shapes diverge within a group, the framework collapses the divergent sub-tree so top-level columns (names) still align even when deeper details differ.

Member kinds the formatter currently understands:

| Kind | Top-level cells |
|------|---------|
| **Port** (`ImplicitAnsiPort`) | direction \| type (keyword + packed dims) \| name |
| **PortConnection** (`.name(expr)`) | identifier \| `(expr)` |
| **ParamAssignment** (`.PARAM(expr)`) | identifier \| `(expr)` |
| **NamedArgument** (`.name(expr)`) | identifier \| `(expr)` |
| **Assignment** (`a = expr;`) | LHS \| `= expr;` |
| **CaseItem** (`expr: stmt`) | labels \| `: clause` |
| **StructPattern** (`key: expr`) | key \| `: expr` |

New kinds can be added by lowering alignment anchors in `src/Layout.cpp`.
`src/FormatDocument.cpp` groups compatible anchors and computes their padding.

### Group Breaking

Consecutive alignable members form a group. Groups are broken intentionally so that alignment doesn't span unrelated sections of code.

**Blank lines** are the primary way to control groups. Structural alignments (ports, instance/param connections, case items, struct fields, struct-pattern assigns) require **two** consecutive blank lines to split — a single blank line is preserved visually but keeps the group aligned. Body alignments (assignment statements, local declarations) use the user-configurable `alignment.linesBetweenGroups` setting (default: `1`), so a single blank line is enough to split assignment groups inside an always/initial/function body.

Standalone comment regions also contribute to that threshold, but their first physical line is free: an N-line comment region counts as N-1 separator lines. A one-line annotation such as `// meta: packet_t` therefore does not disturb alignment; a longer section header can combine with blank lines to split the group.

All alignable lists treat preserved blank/comment gaps below that threshold as soft boundaries. The columns continue across the gap when every row still fits; when shared alignment would add overflow or line breaks, the formatter partitions at a soft boundary and aligns the sections independently.

```systemverilog
// Structural — single blank kept, group stays aligned:
input  logic [7:0]  data_in,
input  logic        valid,

output logic [15:0] data_out,

// Two blank lines start a new structural group:
input  logic clk,
input  logic rst
```

```systemverilog
always_comb begin
    // Body — single blank line splits the group:
    short = 1;
    longer_name = 2;

    // Second group aligns independently:
    a = 3;
    bb = 4;
end
```

**Structural interruptions** — preprocessor directives, macro invocations, and comments attached to directives — always break a group, since alignment columns can't meaningfully span across conditional code.

**Kind changes** also break groups. A port declaration followed by a data declaration starts a new group, since they have different column layouts.

**Maximum padding** (`alignment.maxSpaces`, default: `null`) optionally splits a group when aligning would require an excessive gap. By default no gap-based splitting happens — all otherwise-compatible rows align together. Setting `maxSpaces` to a positive integer reinstates the cap: when any column would need that many or more spaces of padding for a new member, that member starts a fresh group instead. This is useful when one member has a much longer type or name than the rest and forcing every other line to pad out to match would produce unreadable code:

```systemverilog
// Default (maxSpaces = null) — everything aligns:
logic [VERY_LONG_PARAMETER-1:0] some_very_long_signal_name;
logic                           x;

// With maxSpaces = 30, 'x' would start its own group because including it
// would force a >30-char gap in the type column.
```
