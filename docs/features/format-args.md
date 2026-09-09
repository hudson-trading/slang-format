# slang-format Command Reference

## Usage

```
slang-format [options] [<file> ...]
```

If no files are specified, reads from stdin and writes to stdout.
If files are specified without `-i`, prints formatted output to stdout.
With `-i`, modifies files in-place. Multiple files require `-i`.

See [validation](format-validation.md) for checks, rejected files, and exit statuses,
and [formatting markers](format-markers.md) for preserving source formatting.

## Options

### `-h`, `--help`

Display help message and exit.

---

### `--version`

Display version information and exit.

---

### `-i`, `--inplace`

Edit files in-place. Required when formatting multiple files. Cannot be used with stdin.

---

### `-f`, `--force`

Use the formatter's output despite any validation failure, including Git merge
conflict markers. Diagnostics are still printed to stderr, and the command exits
with status 1 when validation fails. See [validation](format-validation.md).

`--force` overrides validation, while `--dry-run` still suppresses writes and
explicit formatting markers remain respected.

---

### `-n`, `--dry-run`

Run formatting and validation without writing files or source to stdout. Can be
used with multiple files and combined with `--force` to check forced formatting.

---

### `--config <path>`

Path to a JSON config file. If not specified, `slang-format` searches upward from the target path, then from the current directory, for `.slang/format.json`.

See [format configuration](format-config.md) for config file options.

---

### `--dump-config`

Print the current configuration as JSON and exit. Useful for inspecting defaults or verifying which config file is being picked up.

---

### `--stage <layout|aligned>`

Select the last formatter pass to run. `layout` performs normalization, spacing, and line breaking without cross-row column alignment. `aligned` (the default) also runs the alignment pass. Both stages run CST validation and idempotency checks independently.

---

### `-j`, `--jobs <n>`

Number of parallel formatting jobs. Defaults to the number of CPU cores. Only relevant when formatting multiple files with `-i`.

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
