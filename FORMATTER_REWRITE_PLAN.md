# Formatter Full-Rewrite Plan

## Goal

Replace the existing renderer entirely with three explicit passes. Preserve the validation machinery, configuration, pure spacing rules where useful, and the test corpus. Do not retain legacy rendering fallbacks.

## Pass 1: Normalize

Build a lossless formatter-owned tree from the CST:

- Wrap every source token with syntax context and normalized trivia.
- Attach same-line comments to the token or directive they trail.
- Represent standalone comments and blank lines as separate nodes.
- Preserve macros, skipped syntax, recovery tokens, and format-off regions as explicit verbatim nodes.
- Materialize nested `ifdef` / `ifndef` / `elsif` / `else` / `endif` chains as synthetic syntax nodes with branches.
- Keep unsafe inactive branches as opaque nodes; safely parsed branches become ordinary normalized children.
- Produce no output and make no layout decisions.

Unit-test token/comment ownership and conditional nesting independently.

## Pass 2: Layout

Lower the entire normalized tree, not selected member kinds, into document IR:

- `Text` / wrapped token
- `SoftLine`
- `HardLine`
- `ConsistentGroup`
- nested layout nodes
- relative indentation anchors
- alignment anchors
- verbatim regions

All syntax-specific formatters must emit this IR. They must not append directly to a string.

### DP line solver

Solve each complete member as one problem:

- Give every soft line a priority based on distance from leaf syntax.
- Lower priority means a more structural/outer break.
- Same-precedence operator chains share a priority.
- Admit deeper priorities only when shallower breaks cannot satisfy the width.
- Use a lexicographic cost:

  1. worst overflow
  2. total overflow
  3. number of overflowing lines
  4. number of breaks
  5. raggedness

- Atomic tokens remain unsplittable, but the solver should still split around an overflowing token.
- Dynamic lists use consistent groups: all comma boundaries split or none do.
- Do not partially bin-pack lists.

### Indentation

Operator continuations hang one indent past the outer expression's starting column, not the member's block indent:

```systemverilog
always_comb result = pkg::word_t'(pkg::word_t'(index)
                                     * pkg::word_t'(count)
                                     + pkg::word_t'(offset));
```

The `*` and `+` lines start four columns past the first `pkg::word_t`.

Expression depth affects break priority, not indentation. Assignment-boundary and list indentation should use explicit anchors rather than inferred absolute columns.

`--stage layout` renders this pass directly. It must be independently CST-equivalent and idempotent.

## Pass 3: Align

Consume the completed layout document and its chosen line breaks, not the original CST through another general formatter run.

- Form groups from alignment anchors already embedded in the IR.
- Compute the padding required by each candidate group.
- Re-solve affected rows when alignment reduces available width.
- Alignment may introduce additional splits.
- Alignment may never remove a split chosen by layout.
- Consider blank/comment boundaries as possible group partitions.
- Choose partitions based on overflow and added-line cost.
- Apply alignment as whitespace/layout transformations only; token and comment ownership remains unchanged.

`--stage aligned` is the full default pipeline.

## Required Architecture Constraints

- No legacy `ListEntry` trivia ownership.
- No direct string emission from syntax visitors.
- No mutable global wrapping state.
- No CST trial-rendering to compare aligned and unaligned rows.
- No syntax-kind migration guards or fallback renderer.
- Recovery and macro cases remain in the same pipeline through verbatim IR nodes.
- Formatting decisions must be deterministic from normalized syntax plus configuration.

## Validation to Retain

Keep the existing validation framework and adapt it to the new pipeline:

- Validate layout output against the original token/CST stream.
- Validate aligned output independently.
- Check idempotency independently for both stages.
- Ensure aligned line breaks are a superset of layout breaks.
- Keep dropped-token, structural-imbalance, parse-error, and generated-file handling.
- Continue using paired goldens:

  - `name.layout.out.sv`
  - `name.out.sv`

## Regression Tests

Add minimal generic tests for:

- Expression-relative continuation indentation.
- Outer-before-leaf DP priority.
- Same-precedence chains.
- Consistent list splitting.
- Atomic-token overflow.
- Trailing versus standalone comment ownership.
- Nested synthetic conditionals.
- Safe and opaque inactive branches.
- Alignment causing further splits.
- Alignment-group partitioning.
- Alignment never rejoining layout lines.

Do not copy proprietary source into tests. Reduce every issue to a minimal test with generic identifiers.

## Execution Order

1. Preserve validation, configuration, `FormatStyle`, CLI behavior, and test inputs.
2. Define the complete normalized and layout IR APIs.
3. Implement normalization and its unit tests.
4. Implement pure token spacing and IR rendering.
5. Port every syntax renderer to emit IR.
6. Implement the member-local DP solver.
7. Implement synthetic conditional formatting and verbatim recovery nodes.
8. Make layout output pass all goldens and idempotency checks.
9. Implement alignment solely over layout documents.
10. Regenerate and review both golden sets.
11. Run the full formatter suite.
12. Run:

    ```bash
    build/bin/slang-format /path/to/large/systemverilog/source/tree -n -j8
    ```

The current acceptance baseline is four CST mismatches and 18 idempotency failures. Completion requires both counts to reach zero, aside from files skipped for existing parse errors.

## Definition of Done

- Every syntax path lowers through the new formatter-owned IR.
- Layout and alignment are independently testable passes with explicit contracts.
- There is no legacy fallback renderer.
- Both stages are CST-equivalent and idempotent across the formatter test suite.
- The large-corpus dry run reports zero CST mismatches and zero idempotency failures.
