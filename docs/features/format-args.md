# Command Line Reference

## Usage

```
slang-format [options] [<file-or-dir> ...]
```

If no files are specified, or the sole input is `-`, reads stdin and writes stdout.
Stdin cannot be combined with other inputs or `-i`.
If files are specified without `-i`, prints formatted output to stdout.
With `-i`, modifies files in-place. Multiple files require `-i`, `--dry-run`, or `--check`.
Directories recursively collect `.sv`, `.svh`, `.v`, and `.vh` files. Each directory
argument's config controls `excludeDirectoryNames` (directory names) and
`projectPaths` (files or subtrees when targeting the project root containing
`.slang/`). Each collected file resolves its own formatting config.

See [validation](format-validation.md) for checks, rejected files, and exit statuses,
and [disabling formatting](format-markers.md) for preserving source formatting.

## Options

### `-h`, `--help`

Display help message and exit.

---

### `--version`

Display version information and exit.

---

### `-i`, `--inplace`

Edit files in-place. Cannot be used with stdin. Unchanged files retain their
contents and timestamps. Changed files are written completely to a temporary file
in the destination directory, then renamed over the destination. A failed write
leaves the original in place. Symlinks remain symlinks and their targets are
updated; permission bits are preserved. Replacement creates a new file, so other
hard links retain the old contents, and ownership/extended metadata are not copied.
The destination directory must be writable. Read-only files are rejected when
changes are needed.

Batch summaries count actual changed, unchanged, generated/excluded, and failed
files. `--dry-run` counts only actual changes as "would format". Failed counts
include validation failures even when the default exit policy tolerates a skip;
forced failed candidates can count as both changed and failed.

---

### `-f`, `--force`

Use the formatter's output despite any validation failure, including Git merge
conflict markers. Diagnostics are still printed to stderr, and the command exits
with status 1 when validation fails. See [validation](format-validation.md).

`--force` overrides validation, while `--dry-run` still suppresses writes and
explicit formatting markers remain respected.

---

### `-n`, `--dry-run`

Run formatting and validation without writing source files or source to stdout. Reports
files that need formatting on stderr, but differences alone return status 0. Can be
used with multiple files and combined with `--force` to check forced formatting.

---

### `--check`, `--verify`

Check that files are already formatted and pass validation. Return 0 on success,
1 if any file needs formatting, fails validation, or cannot be processed.
Supports stdin, multiple files, and directories. Never writes source files or source
to stdout, including with `-i` or `--force`. An explicit `--stats-csv` report is still written. Generated files and explicitly disabled
formatting regions are respected. Batch results do not depend on file order.

### `--Werror`

Treat diagnostic warnings as failures. With `--dry-run`, also fail when formatting
would change a file. `--dry-run --Werror` is the stricter, clang-format-compatible
check spelling; unlike `--check`, it also fails on unmatched `on` warnings.
Without `--dry-run`, this changes exit status without suppressing normal output.

---

### `--strict`, `--fail-on-incomplete-format`

Return status 1 on every validation failure, including structural parse failures,
CST mismatches, and non-idempotent output that normally preserve the input and
return 0. Output handling is unchanged: rejected candidates remain unapplied unless
`--force` is also given. Formatting differences and unmatched `on` warnings alone
are not failures. This is validation of formatting safety, not compilation.

`--failsafe_success=false` is an equivalent spelling for Verible integrations.
The default is `true`; it cannot override `--strict`, `--check`, or `--Werror`.

---

### `--config <path>`

Path to a JSON config file. If not specified, `slang-format` searches upward from each file independently, then from the current directory, for `.slang/format.json`. An explicit config applies to every file. Nested configs replace parent configs; missing settings use defaults. Unknown keys are errors, including nested keys.

See [Configuration](format-config.md) for config file options.

---

### `--assume-filename <path>`, `--stdin_name <path>`

Give stdin a filename for config discovery and diagnostic locations. The file need
not exist, so unsaved editor buffers work. Relative paths resolve against the
current directory. Searches for `.slang/format.json` from the filename's parent,
then falls back to the current directory. `--config` still takes precedence.
Ignored for actual file inputs. Without this option stdin uses the current
directory's config and diagnostics identify `<stdin>`.

```sh
slang-format --assume-filename rtl/top.sv - < editor-buffer.sv
```

---

### `--dump-config`

Print the configuration for the first target (or stdin's assumed filename) as JSON and exit. Useful for inspecting defaults or verifying which config file is being picked up.

---

### `--stage <layout|aligned>`

Select the last formatter pass to run. `layout` performs normalization, spacing, and line breaking without cross-row column alignment. `aligned` (the default) also runs the alignment pass. Both stages run CST validation and idempotency checks independently.

---

### `-j`, `--jobs <n>`

Number of parallel formatting jobs. Defaults to the number of CPU cores. Used for file batches, including checks and dry runs.

### `-v`, `--verbose`

Report `formatting <path> ...` when a worker starts reading and formatting a file,
then `finished <path> (12.345 ms)` after its result has been handled. Starts without matching
finishes identify outstanding work when investigating a stall. `finished` also
appears for rejected or failed files; diagnostics and the summary report failures.

Formatting runs in parallel using `--jobs` (or the default CPU count). Workers
queue progress events; only the main thread prints progress and diagnostics and
applies results. Events are reported as they arrive, so a stalled worker does not
hold up reports from other workers. Output order can vary between runs. Use `-j1`
to isolate one active file at a time. Single-file and stdin modes also report progress.
Elapsed time measures wall-clock milliseconds spent reading, formatting, and
validating that input, including blocked reads. It excludes config discovery,
worker queue wait, result reporting, and output writes. For stdin it includes
waiting for input/EOF. Timings use a monotonic clock.

### `--stats-csv <path>`

Write per-file timing and outcomes to a CSV file, independently of `--verbose`.
Works for files, directory batches, stdin, both formatter stages, checks, and dry
runs. An explicitly requested report is written even when source writes are disabled
by `--check` or `--dry-run`. Source stdout remains unchanged.

The report has these columns:

| Column | Meaning |
| --- | --- |
| `path` | Input filename, or `<stdin>` / `--assume-filename` for stdin. |
| `elapsed_ms` | Read, format, and validation wall time in milliseconds, with three decimals; same measurement as verbose output. |
| `input_bytes` | Number of source bytes read, or zero when input was not read. |
| `status` | `changed`, `unchanged`, `excluded`, `skipped`, or `error`. |
| `validation_failed` | `true` if formatter validation failed; otherwise `false`. |

`changed` means output changed, or would change in a check/dry run. `excluded`
identifies generated files; unchanged disabled regions are `unchanged`. `skipped`
means validation kept the original. `error` covers aborts and file/config/write
failures. Forced failed output can be `changed` with `validation_failed=true`.
These fields describe outcomes, not the command's exit status: strictness and
checking flags still control that status.

An existing report is overwritten. Rows are written and flushed by the main thread
as results are handled, in completion order, so finished rows remain available if
another worker stalls. An interrupted operation has no row. An empty batch writes
only the header. Errors before input processing, such as invalid arguments or a
missing input path, can prevent report creation entirely.

Paths are CSV-quoted with embedded quotes doubled. The report cannot overwrite an
input or loaded configuration, including aliases through symlinks/hard links.
`-` is rejected as a report path. Failure to open the report stops formatting;
a later report write failure makes the command exit 1 while processing continues.

```sh
slang-format -v -j8 --dry-run --stats-csv timings.csv rtl/
```

## Examples

```bash
# Format a file and print to stdout
slang-format top.sv

# Format in-place
slang-format -i top.sv

# Format multiple files in-place with 8 threads
slang-format -i -j 8 src/**/*.sv

# Pipe from stdin
cat top.sv | slang-format

# Check current config
slang-format --dump-config

# Use a specific config file
slang-format --config path/to/format.json top.sv

# Force output despite validation warnings
slang-format -f top.sv

# Inspect independently testable pre-alignment output
slang-format --stage layout top.sv
```

```sh
# CI: fail if any file needs formatting or cannot be validated
slang-format --check rtl/
```
