# Formatting Philosophy

`slang-format` is an **opinionated** formatter — configuration is kept to a minimum, and formatting decisions are driven almost entirely by the concrete syntax tree (CST) rather than existing whitespace or user style. The goal is consistent, readable SystemVerilog with zero bikeshedding.

Some things are preserved from the original source:

- **Macro definitions and argument contents** — these are emitted verbatim since modifying them could change macro expansion, including stringification. Whitespace within an argument in a macro invocation is preserved.
- **Blank lines within lists** — blank lines between module members, port declarations, etc. are kept as authored to respect logical groupings.

## Formatter Passes

Formatting is split into three conceptual passes:

1. **Normalize** wraps source tokens in formatter-owned nodes, attaches same-line comments to the token they trail, separates standalone comments and blank lines, and materializes nested preprocessor conditionals as synthetic branch nodes. Inactive branches are reparsed and formatted recursively only when the complete token stream survives; an unsafe branch alone falls back to opaque, re-indented text.
2. **Layout** applies token spacing and solves member-local soft-line choices. This output is available with `--stage layout` and is independently validated.
3. **Align** groups compatible first-line anchors and adds padding. Alignment is split-only relative to layout: it may retain or add line breaks, but cannot join a member that the layout pass split.

## Lists

Lists are either always vertical, always inline (like array dimensions), or dynamic based on heuristics like the number or length of their children. Bin packing (adding tokens until the line limit is reached) is generally avoided, since it is less readable and creates bad diffs when the list is modified.

### Ports and Instance Connections

Module and interface declaration ports are always vertical. Instance connections
can stay inline when there are at most two connections and they fit within the
column limit. Three or more connections are always vertical, one per line.
Comments, macros, escaped identifiers, and parameter overrides can also force a
shorter connection list to become vertical.

```systemverilog
Child
  u_small (.in(a), .out(b));

Child
  u_large (
    .in    (a),
    .out   (b),
    .ready (ready)
);
```

## Procedural Blocks

A simple assignment or call stays on the same line as the keyword that starts its
procedural block when it fits. This applies to `initial`, `final`, `always`, `always_comb`,
`always_ff`, and `always_latch`. Event and delay controls such as `@(posedge clk)`
and `#5` also keep a simple body inline:

```systemverilog
always_comb value = input_value;

always_ff @(posedge clk) data <= next_data;

initial #5 ready = 1'b1;
```

An `if`/`else` body starts on a new line, indented one level beneath the procedural
keyword or timing control. This applies even without a surrounding `begin`/`end`.
For a `begin`/`end` body, `begin` stays on the header line and the statements inside
are indented:

```systemverilog
always_ff @(posedge clk)
    if (rst)
        valid <= 1'b0;
    else
        valid <= next_valid;

always_comb begin
    sum   = a + b;
    carry = sum > limit;
end
```

## Ifdef Indentation

In member lists, preprocessor conditional directives (`ifdef`/`ifndef`/`else`/`elsif`/`endif`) are indented as structural blocks, not placed flush left at column 0. The content between an `ifdef` and its matching `else`/`endif` is indented one level deeper:

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

This is done for a few reasons:

- These branches are similar to generate branches, which follow normal indentation rules.
- Preprocessor branches can be nested, so indentation helps identify the branch boundaries.

In non-member lists like port lists or lists of case items, ifdefs are dedented one level so that the list items across branches stay aligned. In these contexts, ifdefs tend to act as feature flags toggling individual entries rather than introducing structural blocks:

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

### Why Break Before Operators?

When an expression wraps at a binary operator, the operator starts the
continuation line. This puts the operation and the operand it introduces together:

```systemverilog
assign available = capacity
    - reserved_slots
    - occupied_slots;
```

The left edge becomes a useful guide when scanning a long expression. In a
condition, `&&` and `||` are visible where each continuation begins; in arithmetic,
`+` and `-` make it clear how the following term contributes. Operators at the
ends of lines can be harder to compare when operand lengths vary. Breaking before
`?` and `:` gives conditional expressions the same visual convention.

There is a tradeoff: a trailing operator tells the reader that a line continues
before they move to the next line, and that style is familiar in many codebases.
We prefer making the relationships between wrapped terms easy to scan. This is
a readability choice, not a claim that the alternative is incorrect. Operator
precedence and parentheses still determine grouping; line breaks only help expose
that structure.

### Algorithm

1. Normalize the complete clean member into tokens, atomic text, hard lines, and prioritized soft lines.
2. Try successively deeper priority tiers. The first tier that can satisfy the column limit wins.
3. Within a tier, minimize the worst overflow, total overflow, number of breaks, breaks marked as less preferred, and raggedness, in that order.
4. If no layout can fit because an atomic token is itself too wide, minimize the worst overflow and still split around that token rather than leaving one giant line.

Candidate evaluation is bounded by the number of atoms in the member. If a pathological member exhausts that work budget, the solver deterministically takes the remaining breaks in the current priority tier before considering deeper tiers.

Dynamic lists are represented as consistent soft-line groups, so all comma boundaries split together; the solver never bin-packs only part of a list. Lists and specialized ternary layouts continue through their syntax-specific renderers while their alignment anchors are migrated to the shared IR.

For assignments with a binary expression on the right, the break after the assignment
operator competes with breaks in the outer expression chain. Moving the RHS onto its
own indented line is preferred only when it reduces the total number of lines needed
to fit. Equal line counts keep the first operand beside the assignment operator.

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

Continuations in an assignment's outer binary chain align with the first token of
the RHS. If the RHS moves below the assignment operator, the whole chain uses one
indent from the statement. Nested expressions retain their own continuation anchors.

### What Doesn't Wrap

- **Declaration types and names** stay on one line; initializer expressions can wrap.
- **Concatenations** (`{a, b, c}`) and **function arguments** are handled by dynamic list verticalization, not expression wrapping. When an overflowing function call is the sole assignment RHS, the whole call moves to the next indented line; short calls remain inline.
- **Macro invocations** are emitted as raw text and are not individually wrapped.

## Alignment

The formatter supports column-aligned declarations within groups of consecutive members of the same kind. Columns are padded so that identifiers, types, and dimensions line up vertically:

```systemverilog
input  logic [7:0]  data_in,
input  logic        valid,
output logic [15:0] data_out
```

### Alignment Model

Each alignable row produces a tree of `AlignCell`s derived from its syntax. Consecutive compatible rows form a group, and the framework computes maximum widths per column and emits each row with padding between columns. When subtree shapes diverge within a group, the framework collapses the divergent subtree so top-level columns (names) still align even when deeper details differ.

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

**Blank lines** are the primary way to control groups. Two settings control how many existing separator lines split a group:

- `alignment.statementGapLines` (default: `1`) applies to assignment statements and standalone variable, net, and parameter declarations. A single blank line splits these groups by default.
- `alignment.listGapLines` (default: `2`) applies to ports, parameter ports, instance/param connections, case items, struct fields, assignment-pattern fields, and other non-statement rows. A single blank line is preserved visually but keeps these groups aligned by default.

Set both options to `1` to split both kinds of alignment group on a single blank line. Neither option inserts blank lines; values below `1` use `1`.

Standalone comment regions also contribute to that threshold, but their first physical line is free: an N-line comment region counts as N-1 separator lines. A one-line annotation such as `// meta: packet_t` therefore does not disturb alignment; a longer section header can combine with blank lines to split the group.

All alignable lists treat preserved blank/comment gaps below that threshold as soft boundaries. The columns continue across the gap when every row still fits; when shared alignment would add overflow or line breaks, the formatter partitions at a soft boundary and aligns the sections independently.

Module-level one-line procedural blocks also use soft boundaries. Multi-row sections can keep a shared alignment across one, but a singleton section is isolated rather than padded to match a neighboring block.

Trailing comments align while adjacent comment anchors are within one indentation width. A wider jump starts a new comment group so one long row does not push shorter comments far to the right.

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

**Structural interruptions** — preprocessor directives, macro invocations, and comments attached to directives — always break a group, since alignment columns can't meaningfully span conditional code.

**Kind changes** also break groups. A port declaration followed by a data declaration starts a new group, since they have different column layouts.

**Maximum padding** (`alignment.paddingLimit`, default: `null`) optionally splits a group when aligning would require an excessive gap. By default, no gap-based splitting happens — all otherwise compatible rows align together. Setting `paddingLimit` to a positive integer reinstates the cap: when any column would need that many or more spaces of padding for a new member, that member starts a fresh group instead. This is useful when one member has a much longer type or name than the rest and forcing every other line to pad out to match would produce unreadable code:

```systemverilog
// Default (paddingLimit = null) — everything aligns:
logic [VERY_LONG_PARAMETER-1:0] some_very_long_signal_name;
logic                           x;

// With paddingLimit = 30, 'x' would start its own group because including it
// would force a gap of 30 or more spaces in the type column.
```
