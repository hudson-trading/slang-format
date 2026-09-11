# Validation

`slang-format` checks the input before formatting, then validates the formatted
output before the CLI applies it. These checks determine whether the CLI writes
formatted text, keeps the original text, or rejects the file.

## Input checks

### Generated files

A leading comment containing `@generated` excludes the file from formatting.
The original source is preserved, including when `--force` is used. This is a
file exclusion, so it does not produce a validation failure by itself.
See [generated files](format-markers.md#generated-files) for marker placement.

### Git merge conflict markers

The formatter reports the first marker's input line. Markers must start at column
one and contain at least seven repeated `<`, `=`, `>`, or `|` characters. This
includes diff3 base markers and larger custom marker sizes.

Opening, closing, and base markers may have whitespace-separated labels.
Separator lines may have trailing spaces or tabs. An equals-only line is treated
as a separator only when followed by a closing marker of the same width; otherwise
it may be a comment heading underline. A single opening, closing, or base marker
is enough; the formatter does not require a complete conflict block.

Detection reads raw source, so it also catches standalone marker lines inside
comments, multiline strings, and inactive preprocessor branches. Marker text
embedded in an ordinary comment or string, such as `// <<<<<<< HEAD`, is not a
standalone marker line.

Resolve the conflict and rerun the formatter for normal validated output, or use
`--force` to explicitly request a formatting attempt while markers remain.

### Parse structure

The input is parsed before formatting. Parser errors that leave unclosed
constructs indicate that the formatter cannot reliably format the source. This
can reflect incomplete source, parser limitations, or unexpanded macros. Without
`--force`, the CLI keeps the original text and reports the parse errors. The batch
summary labels these files as `skipped (input parse errors)`.

Recovered parser errors alone do not necessarily prevent formatting. The
formatter does not expand macros or follow includes, and it accepts standalone
source fragments. These checks do not establish whether the design compiles.

## Validating formatted output

| Check | Meaning |
| --- | --- |
| Reparse | The formatted output could not be parsed into a syntax tree. |
| Syntax preservation | The formatted token sequence differs from the input, including relevant comments, directives, and recovered source. Reported as a CST mismatch. |
| Idempotency | Formatting the result again changes it. |

Both `--stage layout` and `--stage aligned` run validation. The aligned stage also
checks that its intermediate layout output preserves the input token sequence.

An exception during formatting or validation is reported as an internal error.

## Output and exit status

Without `--force`, the CLI handles each file as follows:

| Result | stdout mode | In-place mode | Exit status |
| --- | --- | --- | --- |
| Input and output checks pass | Formatted source | Write formatted source | `0` |
| Generated file | Original source | Leave unchanged | `0` |
| Input parse structure failure | Original source | Leave unchanged | `0` |
| CST or idempotency failure | Original source | Leave unchanged | `0` |
| Merge conflict, failed reparse, or internal error | No source output | Leave unchanged | `1` |

Skipped input or output checks still produce diagnostics on stderr. A rejected file
does **not** stop other files in a batch from being formatted. The command exits
with status `1` if any file is rejected, a forced result fails input or output checks,
or a file operation fails.

Batch outcome counts are exclusive: skipped files are not also counted as failed.
Input parse skips and output validation skips have separate reason labels. Aborts,
configuration errors, and file I/O errors count as failed. For example:

```text
formatted 0 files, 3998 unchanged, 0 excluded, 3 skipped (input parse errors), 0 failed
```

These counts describe what happened to each file, independently of exit policy.
`--strict` can make a skipped file fail the command without changing its outcome.

An unmatched `slang-format: on` produces a warning and formatting continues.
The warning alone does not change the exit status. An unmatched `off` silently
preserves the rest of its list scope, allowing a top-of-file `off` to disable
the whole file.
See [disabling formatting](format-markers.md) for scope rules.

`--dry-run` performs the same checks and uses the same exit status rules, but
never writes source files or emits source on stdout. It reports formatting differences
on stderr without failing for those differences alone.

`--check` (alias `--verify`) also suppresses source writes and output, but
returns `1` for any formatting difference or validation failure, including failures
that normally preserve the original and return `0`. It returns `0` for unchanged
valid source and intentional exclusions. Unmatched `on` warnings alone still pass.
`--dry-run --Werror` additionally fails on diagnostic warnings. Neither mode lets
`--force` turn a failed validation into a successful check.

## Strict validation

`--strict` returns `1` for every validation failure while preserving the normal
output policy. Structural parse failures, CST mismatches, and idempotency failures
therefore keep the original source but fail the command. This also applies to
stdin and batches; a later successful file never clears an earlier failure.

`--fail-on-incomplete-format` and `--failsafe_success=false` are equivalent
spellings. Explicit `--strict` and checking options take precedence over
`--failsafe_success=true`. Recovered parser errors that do not invalidate
formatting, intentional exclusions, and unmatched `on` warnings still pass.

```sh
# Require safe formatting without forcing rejected candidates
slang-format --strict -i rtl/
```

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
and [disabling formatting](format-markers.md).

An explicit `--stats-csv <path>` report is written in every formatting mode,
including checks and dry runs. Its write failures make the command exit `1`.
See [command-line statistics](format-args.md#-stats-csv-path) for timing and outcome fields.
