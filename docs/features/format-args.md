# slang-format Command Reference

## Usage

```
slang-format [options] [<file> ...]
```

If no files are specified, reads from stdin and writes to stdout.
If files are specified without `-i`, prints formatted output to stdout.
With `-i`, modifies files in-place. Multiple files require `-i`.

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

Force output even if CST validation fails. Validation errors are downgraded to warnings but still printed to stderr.

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
