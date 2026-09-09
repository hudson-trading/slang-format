# Validation

`slang-format` checks its output before the CLI applies it. A diagnostic explains
what failed; output handling determines whether the CLI writes the formatted
text, keeps the original text, or rejects the file.

## What is checked

| Check | Meaning |
| --- | --- |
| Git merge conflict markers | The input contains a standalone conflict marker line. |
| Parse structure | Parser errors left unclosed constructs, so the formatter cannot reliably format the source. |
| Reparse | The formatted output could not be parsed into a syntax tree. |
| Syntax preservation | The formatted token sequence differs from the input, including relevant comments, directives, and recovered source. Reported as a CST mismatch. |
| Idempotency | Formatting the result again changes it. |
| Internal error | The formatter threw an exception. |

Both `--stage layout` and `--stage aligned` run validation. The aligned stage also
checks that its intermediate layout output preserves the input token sequence.

These checks validate formatting, not whether the design compiles. The formatter
does not expand macros or follow includes, and it accepts standalone source
fragments. Recovered parser errors alone do not necessarily prevent formatting.

## Output and exit status

Without `--force`, the CLI handles each file as follows:

| Result | stdout mode | In-place mode | Exit status |
| --- | --- | --- | --- |
| Validation passes | Formatted source | Write formatted source | `0` |
| Generated file | Original source | Leave unchanged | `0` |
| Parse structure, CST, or idempotency failure | Original source | Leave unchanged | `0` |
| Merge conflict, failed reparse, or internal error | No source output | Leave unchanged | `1` |

Skipped validation failures still produce diagnostics on stderr. A rejected file
does **not** stop other files in a batch from being formatted. The command exits
with status `1` if any file is rejected, a forced result has validation failures,
or a file operation fails.

`--dry-run` performs the same checks and uses the same exit status rules, but
never writes files or emits source on stdout.

## Forcing output

`--force` overrides **every validation failure**, including merge conflicts:

```sh
# Inspect the candidate despite validation failures
slang-format --force top.sv

# Apply it in place
slang-format --force -i top.sv
```

Diagnostics remain visible and the exit status is still `1` if validation failed.
Force permits output; it does not make that output pass validation. Conflict
markers and the surrounding source may be reformatted.

If an exception prevents the formatter from producing a replacement, the result
falls back to the original source. Force cannot complete an operation that failed
to read or write a file. It also respects `--dry-run`, generated-file exclusions,
and [formatting markers](format-markers.md).

## Git merge conflict markers

The formatter reports the first marker's input line. Markers must start at column
one and contain at least seven repeated `<`, `=`, `>`, or `|` characters. This
includes diff3 base markers and larger custom marker sizes.

Opening, closing, and base markers may have whitespace-separated labels.
Separator lines may have trailing spaces or tabs. A single marker is enough;
the formatter does not require a complete conflict block.

Detection reads raw source, so it also catches standalone marker lines inside
comments, multiline strings, and inactive preprocessor branches. Marker text
embedded in an ordinary comment or string, such as `// <<<<<<< HEAD`, is not a
standalone marker line.

Resolve the conflict and rerun the formatter for normal validated output, or use
`--force` to explicitly request a formatting attempt while markers remain.
