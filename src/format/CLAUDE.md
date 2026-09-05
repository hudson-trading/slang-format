# Formatter Development Guide

## Design Philosophy

Opinionated formatter for SystemVerilog with minimal configuration. Formatting is almost fully determined by the CST (concrete syntax tree) rather than existing trivia. Exceptions: macro definitions and empty lines within syntax lists are preserved.

- No bin packing for lists -- each item gets its own line based on list kind, length, and item count.
- Line wrapping: clean binary/property/sequence members are lowered to hierarchical formatter IR and solved with priority-tier dynamic programming. Outer soft lines are preferred over lines near leaf expressions.
- Data declarations are not split; just keep them on the same line.

## Rewrite Bootstrap

The formatter core is intentionally a pass-through scaffold. Read
[`FORMATTER_REWRITE_PLAN.md`](../../FORMATTER_REWRITE_PLAN.md) before adding
renderer logic.

The retained foundation is:

- `FormatValidation.cpp` and `CstValidation.cpp` for parsing, CST safety, and idempotency checks.
- `formatter_main.cpp` for CLI, config discovery, file traversal, and stage selection.
- `FormatConfig.h` / `FormatConfig.cpp`, `FormatStyle.cpp`, and `FormatterUtils.h` for reusable configuration, policy, and trivia helpers.
- `Formatter.h` / `Formatter.cpp` as the minimal public entry point to replace.
- `scripts/test_format.py` and `tests/format/` for layout and aligned golden coverage.

The old renderer files were removed rather than retained as fallback paths.

## Slang Syntax Tree Model

### Tree Structure

The syntax tree is a concrete syntax tree (CST) -- it preserves all source text including whitespace and comments as trivia. Every token and piece of whitespace is accounted for.

```
ModuleDeclaration (SyntaxNode)
  ├── ModuleHeader (SyntaxNode)
  │   ├── "module" (Token)
  │   ├── "top" (Token)
  │   ├── AnsiPortList (SyntaxNode)
  │   │   ├── "(" (Token)
  │   │   ├── SeparatedList<MemberSyntax> (list node)
  │   │   │   ├── ImplicitAnsiPort (SyntaxNode)
  │   │   │   ├── "," (Token - separator)
  │   │   │   └── ImplicitAnsiPort (SyntaxNode)
  │   │   └── ")" (Token)
  │   └── ";" (Token)
  ├── SyntaxList<MemberSyntax> (list node -- module body)
  │   ├── DataDeclaration
  │   ├── AlwaysBlock
  │   └── ContinuousAssign
  └── "endmodule" (Token)
```

### SyntaxNode

Defined in `external/slang/include/slang/syntax/SyntaxNode.h`.

Key members:
- `kind` -- `SyntaxKind` enum identifying the node type
- `parent` -- pointer to parent node
- `getChildCount()` -- number of direct children
- `childNode(index)` -- returns child as `SyntaxNode*` (nullptr if child is a token)
- `childToken(index)` -- returns child as `Token` (empty if child is a node)
- `tokens_begin()` / `tokens_end()` -- flat iterator over ALL tokens in subtree (depth-first)
- `getFirstToken()` / `getLastToken()` -- first/last token in subtree
- `sourceRange()` -- source span of entire subtree
- `as<T>()` / `as_if<T>()` -- safe casting to concrete syntax types

Child iteration pattern:
```cpp
for (size_t i = 0; i < node.getChildCount(); i++) {
    if (auto* child = node.childNode(i)) {
        // child is a SyntaxNode
    } else {
        Token tok = node.childToken(i);
        if (tok) { /* child is a Token */ }
    }
}
```

**Important:** `getChild(index)` is **private**. Use `childNode()` / `childToken()` instead.

### List Nodes

All list types derive from `SyntaxListBase`. Detect with `SyntaxListBase::isKind(node.kind)`.

Three list kinds (first three values in `SyntaxKind` enum):
- **`SyntaxList`** -- homogeneous list of syntax nodes (module members, statements)
- **`TokenList`** -- list of tokens only
- **`SeparatedList`** -- items interleaved with separator tokens (commas)

`SeparatedSyntaxList<T>` layout: `[item, separator, item, separator, ..., item]`
- `size()` returns item count (not including separators)
- `operator[](n)` returns the nth item (skipping separators)
- `elems()` returns raw span including separators
- Every odd-indexed element is a separator token

List nodes are the key structural element for indentation -- module bodies, statement blocks, port lists, etc. are all represented as list children of their parent node.

### Token

Defined in `external/slang/include/slang/parsing/Token.h`.

Key members:
- `kind` -- `TokenKind` enum (352 kinds: keywords, operators, punctuation, literals, identifiers)
- `rawText()` -- original source text
- `valueText()` -- lexed text (escapes resolved)
- `trivia()` -- `std::span<Trivia const>` of leading trivia
- `isMissing()` -- true if token was inserted by error recovery
- `isOnSameLine()` -- checks trivia for newlines
- `operator bool()` -- true if token is valid (has info)

### Trivia

Trivia is **leading only** -- attached to the token that follows it. Access via `token.trivia()`.

`TriviaKind` enum (9 kinds):
- `Whitespace` -- spaces and tabs
- `EndOfLine` -- newline characters
- `LineComment` -- `// ...`
- `BlockComment` -- `/* ... */`
- `Directive` -- preprocessor directives (access syntax node via `trivia.syntax()`)
- `DisabledText` -- text inside disabled `ifdef`/`else` branches
- `SkippedTokens` -- tokens skipped during error recovery
- `SkippedSyntax` -- syntax nodes skipped during error recovery

Key methods:
- `getRawText()` -- raw source text of the trivia
- `syntax()` -- for `Directive`/`SkippedSyntax`, returns the syntax node
- `getSkippedTokens()` -- for `SkippedTokens`, returns the skipped tokens

### Macros and Preprocessor

**Important:** The formatter never expands macros. We customize the slang options to use no macro definitions, so all macro invocations are unknown and appear as `TriviaKind::Directive` with `SyntaxKind::MacroUsage` in trivia. There are no expanded tokens to skip.

- **Macro invocations** (`TokenKind::MacroUsage`) appear as `TriviaKind::Directive` trivia with `SyntaxKind::MacroUsage`. Since macros are never expanded, the macro text is emitted directly from the directive's source range.
- **Macro definitions** (`\`define`) appear as `TriviaKind::Directive` trivia attached to the next real token. The directive's syntax node is accessible via `trivia.syntax()`.
- **Conditional directives** (`\`ifdef`/`\`else`/`\`endif`) also appear as directive trivia. Disabled branches produce `TriviaKind::DisabledText`.

## Deleted Renderer Reference

The remainder of this section describes the removed renderer only as behavioral
background. Do not recreate its mutable output state or mixed layout/alignment
pipeline.

### File Layout

The formatter is split across several files in `src/format/`:

- **`Formatter.cpp`** -- Core formatting pipeline: `format()`, `formatNode()`, `formatExpression()`, `formatVerticalList()`, trivia processing (`appendTrivia`, `appendToken`, `processLeadingMemberTrivia`), list tokenization (`tokenizeListTrivia`, `classifyTrivia`), and output helpers (`flushTokenBuffer`, `ensureSpaceOrNewline`, `appendIndent`, `pushToken`).
- **`Formatter_special.cpp`** -- `formatSpecial()` overloads for syntax types with mixed body regions that can't be handled by the generic `formatNode` loop (if/else, hierarchy instantiations, property/sequence declarations).
- **`Formatter_ifdefs.cpp`** -- `formatConditionalDirective()` for ifdef/else/elsif/endif indentation and `emitDisabledText()` for inactive branch content.
- **`Formatter_wrap.cpp`** -- Integration between syntax-specific wrapping and the formatter-IR member solver, including conservative migration guards for recovery, lists, comments, and ternaries.
- **`FormatIR.cpp`** -- First-pass token/trivia normalization, synthetic conditional nodes, hierarchical formatter IR, consistent list groups, and the member-local DP line solver.
- **`FormatStage.h`** -- Selects independently renderable `Layout` or full `Aligned` output.
- **`Formatter_align.cpp`** -- Tree-of-cells alignment for vertical list members. Per-syntax-kind `produceFor*` helpers walk each member's syntax to build an `AlignCell` tree; `buildAlignGroups` collapses divergent sub-trees and computes per-column max widths; `formatAlignedRow` emits a row with padding between cells. Types live in `include/format/Formatter_align.h`.
- **`FormatStyle.cpp`** -- Pure decision functions: `shouldInsertWhitespace()` (token spacing), `getListStyle()` (generated, determines Inline/Dynamic/Vertical per syntax kind), `nodeNeedsOwnLine()`, `isOpenDelim()`/`isCloseDelim()`.
- **`FormatValidation.cpp`** -- Post-format CST equivalence checking.

Headers are in `include/format/`:
- **`Formatter.h`** -- Class definition with all state, `BufferedToken`, `ListEntry`, public/private method declarations.
- **`FormatStyle.h`** -- `ListStyle` enum, function declarations for style decisions.
- **`FormatConfig.h` / `FormatConfig.cpp`** -- `Config` and `AlignConfig` plus JSON loading and config discovery shared by all users of `slang_format_lib`.
- **`FormatterUtils.h`** -- `isCommentTrivia()`/`isLineCommentTrivia()` helpers that handle the slang preprocessor's rewriting of comment trivia to `DisabledText`.

### Key Types

```cpp
// Token with its syntactic context, used for buffering and spacing decisions.
struct BufferedToken {
    Token token;
    SyntaxKind parentKind = SyntaxKind::Unknown;  // which syntax node this token belongs to
    bool noSpaceBefore = false;                    // set by compactExpr (bracket contents)
    bool inDataType = false;                       // inside a DataTypeSyntax (type dims get spaces)
};

// One entry in a pre-tokenized vertical list. Flat structure: blank lines,
// comments, directives, and macros sit alongside actual members.
struct ListEntry {
    enum Kind { Member, BlankLine, LineComment, BlockComment, Directive, MacroUsage, RawTrivia };
    Kind kind;
    const SyntaxNode* node = nullptr;       // Member: the syntax node
    Token separatorToken;                    // Member: comma for separated lists
    bool hasMacroTrailingToken = false;      // Member: real token follows a macro on same line
    bool groupWithPrevious = false;          // Member: port inherits type from previous
    Trivia trivia;                           // Comment/RawTrivia: the trivia item
    bool isTrailing = false;                 // Comment: trailing on previous member's line
    const SyntaxNode* syntaxNode = nullptr;  // Directive/MacroUsage: the directive syntax
};

// State of the output cursor
enum class LineState {
    Default,           // mid-line after a token or space
    NeedNewline,       // next content must start on a new line
    LineStart,         // a \n was just emitted; need indent before content
    AfterBlockComment, // block comment emitted; respect next EndOfLine
    AfterDirective,    // directive emitted; allow trailing comment inline
};
```

### Formatting Pipeline

The high-level flow for formatting a file:

1. **Normalize** -- `format(rootNode)` first builds `NormalizedFormatDocument`, which owns trailing-comment attachment and synthetic conditional branches.

2. **Layout** -- The normal renderer applies spacing and member-local line decisions. Clean binary/property/sequence members use `FormatDocument`; recovery, list-bearing, commented, and specialized ternary members conservatively use the lossless syntax-specific paths during migration.

3. **Align** -- Unless `FormatStage::Layout` was requested, vertical lists form alignment groups and add first-line padding. Alignment may add splits but must not remove layout-stage splits.

4. **Root emission** -- For `CompilationUnit`, calls `formatVerticalList(root.members)` then processes EOF trivia. For reparsed snippets, calls `formatMember(rootNode)`.

5. **`formatNode(node, isMember)`** -- The main recursive function. Iterates direct children:
   - **Token children**: `pushToken` into `tokenBuffer`, managing `compactExpr` state on bracket open/close.
   - **List children**: Resolves `ListStyle` (from `getListStyle()`). Dynamic lists check for comments (`listHasComments`) and measure inline width (`measureInlineWidth` + `currentColumn()` vs `columnLimit`). Vertical lists flush the buffer and call `formatVerticalList` with `frame.blockIndented()`. Inline lists recurse with `formatNode`.
   - **Expression children**: Delegate to `formatExpression`.
   - **Other nodes**: Recurse at same indent level, or dispatch via `SpecialDispatcher` to `formatSpecial` overloads.
   - If `isMember`, calls `flushTokenBuffer(true)` at the end.

6. **`formatExpression(expr)`** -- Sets `currentNodeKind` and `inDataType`. Special-cases `InvocationExpression` (temporarily clears `compactExpr` for arguments). Clean binary/property/sequence expressions can be lowered to `FormatDocument`; other expressions use their syntax-specific path.

7. **`formatVerticalList(list)`** -- Two-phase approach:
   - **Phase 1: Tokenize** -- `tokenizeListTrivia()` walks the list and closing token's trivia, building a flat `SmallVector<ListEntry>`. Uses `classifyTrivia()` to categorize each trivia item (blank lines, comments, directives, macros) and `tokenizeDirectiveSubTrivia()` for trivia on directive tokens themselves. Macro-expanded members only process `Directive` trivia to avoid leaking macro-body artifacts.
   - **Phase 2: Emit** -- Iterates `ListEntry` items in order:
     - `BlankLine`: emit `\n`
     - `LineComment`/`BlockComment`: emit with `ensureSpaceOrNewline()`, handle trailing vs standalone
     - `MacroUsage`: emit source text, check for trivial trailing members (`;` after macro)
     - `Directive`: dispatch to `formatConditionalDirective()` for ifdef/else/endif, or emit raw for others. Handle trailing comments on directive line.
     - `Member`: set `skipNextLeadingTrivia`, call `formatMember()` (or inline `formatNode` for grouped ports), append separator comma, manage `NeedNewline` unless next member is grouped.

8. **`formatSpecial` overloads** (in `Formatter_special.cpp`) -- Handle syntax types where the generic `formatNode` loop doesn't work because they have mixed body regions:
   - `ConditionalStatementSyntax`: if/else with block vs non-block bodies, `else if` chaining, `end else` on same line.
   - `HierarchyInstantiationSyntax`: parameter overrides always vertical, instance connections use Dynamic style.
   - `PropertyDeclarationSyntax` / `SequenceDeclarationSyntax`: header on one line, body indented, end keyword dedented.

### Token Buffer and Emission

Tokens are not emitted directly. Instead they're buffered via `pushToken()` into `tokenBuffer`, recording `parentKind`, `noSpaceBefore`, and `inDataType` state at push time. This allows:
- Deferred spacing decisions (the next token's context affects the space before it)
- Column width estimation via `currentColumn()` without emitting
- Batch emission via `flushTokenBuffer()`

**`flushTokenBuffer(isMemberBegin)`** emits all buffered tokens. If `isMemberBegin`, the first token gets `respectNewlines=true` so its leading trivia is processed.

**`appendToken(bt, respectNewlines)`** emits a single token:
1. Processes leading trivia (unless `skipNextLeadingTrivia`) via `appendTrivia`
2. Detects inline conditional directives in trivia and emits them verbatim
3. Skips macro-expanded tokens (their source text comes from the macro invocation trivia)
4. Determines spacing via `shouldInsertWhitespace(leftKind, rightKind, leftParent, rightParent, rightInDataType)`
5. Appends the token text to `result`

**`appendTrivia(trivia, respectNewlines)`** processes a single trivia element, managing `lineState` transitions. Handles whitespace, newlines, line/block comments, preprocessor directives (including conditional directives and macro usage), and skipped tokens.

### Spacing Model

**`shouldInsertWhitespace()`** (in `FormatStyle.cpp`) is a pure function using `TokenKind` and parent `SyntaxKind`:
- No space inside delimiters: `(x)`, `[y]`, `{z}`
- No space before `,` or `;`; always space after `,`
- No space around `.` (member access) or `::` (scope resolution)
- No space after `#`, `@`, or cast apostrophe
- Space before `[` only in data type contexts (`some_t [7:0]`), not expression indexing (`a[3:0]`)
- Colon: space in ternary (`a ? b : c`), no space in ranges/case labels
- No space after prefix unary; no space before postfix unary
- Space before `(` in port lists and instance connections, but not function calls
- `compactExpr` (recorded as `noSpaceBefore` on `BufferedToken`) removes all spaces inside brackets: `signal[7:0]`, `[a+:b]`

### Indentation Model

Indentation is driven by the syntax tree structure rather than individual token matching. When `formatNode` encounters a list child, it recurses with `frame.blockIndented()` (one more block level deep). This naturally handles:
- Module bodies (member list inside `ModuleDeclaration`)
- `begin`/`end` blocks (statement list inside `BlockStatement`)
- Class bodies, package bodies, etc.
- Port lists, parameter lists

The opening/closing tokens (`module`/`endmodule`, `begin`/`end`) stay at the parent indent level since they're sibling tokens of the list, not inside it.

Ifdef/else/endif directives manage their own indentation in `formatConditionalDirective()`: dedent before `else`/`elsif`/`endif`, indent after `ifdef`/`ifndef`/`else`/`elsif`.

### List Style Resolution

`getListStyle()` (generated by `scripts/gen_format_style.py`) returns one of:
- **`Vertical`**: Always one-item-per-line. Module bodies, statement blocks, class bodies, etc.
- **`Dynamic`**: Go vertical if the list contains comments/directives OR if `currentColumn() + measureInlineWidth(list) + trailingWidth > columnLimit`. Used for port lists, argument lists, concatenations, etc.
- **`Inline`**: Always inline. Default for lists not explicitly classified.

Special case: `HierarchicalInstance` connections go Vertical if >1, Dynamic if exactly 1.

### Config

Defined in `include/format/FormatConfig.h`. Uses reflect-cpp for JSON serialization.

**Discovery** (`formatter_main.cpp`): with no `--config`, the CLI searches for
`.slang/format.json` first by walking up from the **target path** being
formatted (the first positional file/dir), then — if nothing is found there —
by walking up from the current working directory. The target tree takes
precedence; CWD is a fallback. This lets `slang-format /some/other/repo/src`
pick up that repo's config, while still working when the target is a throwaway
tempfile outside any project tree (some lint runners copy each source into a
tempfile and run the formatter on it — only CWD reflects the real project
there). Stdin mode (no positional targets) discovers from CWD. An
explicit `--config <path>` overrides discovery entirely. Unknown JSON keys are
ignored (`rfl::DefaultIfMissing`).

**`dirs`** scopes which subtrees get formatted, but *only* when the formatter
is pointed at the **config root** (the directory holding `.slang/format.json`).
In that case the crawl is restricted to the config's `dirs` (resolved relative
to the config root) instead of the whole tree. Pointing the formatter at any
other directory — a subfolder, or an unrelated path — ignores `dirs` and
crawls the target directly, so you can format a subfolder ad hoc without
committing the whole tree. `dirs` only applies to a config found via the
**target** tree — not one found via the CWD fallback or an explicit
`--config` (neither implies the target is a project root). Both discovery and
`dirs` scoping are covered by `scripts/test_config_discovery.py`.

### Documentation

User-facing formatter docs live in `docs/features/`:

- `format-philosophy.md` -- design philosophy, ifdef indentation, line wrapping, alignment (hand-written)
- `format-args.md` -- CLI argument reference (hand-written)
- `format-config.md` -- config file reference (**auto-generated** by `scripts/genconfig.py` from the JSON schema + `--dump-config` defaults; do not edit by hand)

When adding or changing config options in `FormatConfig.h`, run `python3 scripts/genconfig.py` to regenerate the config docs and JSON schema.

## Format Test Runner

`scripts/test_format.py` runs `slang-format` on every `.sv` test twice and compares `--stage layout` with `.layout.out.sv` and `--stage aligned` with `.out.sv`. It verifies CST equivalence and idempotency independently for both stages.

### Test layout

```
tests/format/
  prop/                    # Proprietary test files, ignored by git
  basic_module.sv          # input test case
  basic_module.layout.out.sv # pre-alignment golden (managed by --update)
  basic_module.out.sv      # aligned golden (managed by --update)
```

### Test Case Methodology
If a new proprietary test file is pasted in (tests/format/prop/*), replicate and obfuscate the problematic syntax and iterate on that smaller example.
In general develop in test cases rather than tmp files, and use the test harness to iterate. If there is functionality missing in the harness or inspection script for productive debugging, please add that functionality and document it here. Avoid running slang --cst-json directly: use the test harness and inspect script functionality!!!

**Every specific behavior request gets its own test case.** When the user asks for a concrete formatting rule ("don't expand single-item lists", "align packed dims across heterogeneous rows", "no space after `:` in compact dims", etc.), add or extend a `tests/format/*.sv` test case that exercises the rule — both the positive case (the new behavior) and a contrast case (where the rule should *not* fire) when applicable. The task isn't done until that test case exists and passes; without one, the next refactor will silently regress the behavior.

To inspect or debug, you can use the test harness's flags or `scripts/inspect_cst.py`

### Usage

```bash
# Run all tests
python3 scripts/test_format.py

# Build first, then run
python3 scripts/test_format.py --build

# Update golden outputs after changing formatter behavior
python3 scripts/test_format.py --build --update

# Run a subset of tests (substring match on filename)
python3 scripts/test_format.py ifdef

# Inspect a single test: opens source diff and CST JSON diff in editor
python3 scripts/test_format.py --cst mixed_trivia
```

### Flags

- `filter` -- positional args filter tests by substring match on filename
- `--build` -- build `slang-format` before running tests
- `--update` -- write current formatter output as the new golden files
- every case is run twice: `--stage layout` and `--stage aligned`
- `--cst` -- open editor diffs of source and CST JSON for a single test (for debugging)
- `--exclude-prop` -- skip ignored proprietary smoke tests (used by CTest)

On failure, diffs are automatically opened in the editor set by `$EDITOR` (supports VS Code and vim/nvim).

### CST equivalence

After formatting, the script compares the CST JSON (`--cst-json-mode no-whitespace`) of the original and formatted files. If they differ, the formatter changed the meaning of the code. On mismatch, it writes `.cst.json` / `.out.cst.json` files next to the test and opens diffs in the editor.

## CST Inspector

`scripts/inspect_cst.py` inspects the CST trivia structure of a SystemVerilog file using slang's `--cst-json` output. Useful for debugging how directives, comments, and disabled text attach to tokens.

```bash
# Show all directive trivia (ifdef/else/endif, macro usage, etc.)
python3 scripts/inspect_cst.py tests/format/ifdef_with_comments.sv --directives

# Show trivia on tokens matching a string
python3 scripts/inspect_cst.py tests/format/ifdef_in_body.sv --token assign

# Show all non-whitespace trivia
python3 scripts/inspect_cst.py tests/format/mixed_trivia.sv --all-trivia

# Show directives matching a specific SyntaxKind
python3 scripts/inspect_cst.py tests/format/ifdef_with_comments.sv --kind EndIfDirective
```
