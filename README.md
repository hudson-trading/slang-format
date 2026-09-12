# slang-format

`slang-format` is an opinionated SystemVerilog formatter built on top of the
[slang](https://github.com/MikePopoloski/slang) parser. It provides both a
standalone command-line tool and the `slang::format` CMake library target.

The formatter has its own version and release stream. Language servers and
other tools can pin this repository as a submodule and link it in-process,
while users can independently install a newer `slang-format` binary.

See [GitHub releases](https://github.com/hudson-trading/slang-format/releases)
for published versions and release notes.

See the [documentation](https://hudson-trading.github.io/slang-format/) for
[installation](https://hudson-trading.github.io/slang-format/start/installing/),
[workflow integration](https://hudson-trading.github.io/slang-format/start/workflow-integration/),
[configuration](https://hudson-trading.github.io/slang-format/features/configuration/), and
[embedding](https://hudson-trading.github.io/slang-format/start/workflow-integration/#embedding).

## Usage

Format all Verilog and SystemVerilog files in a directory tree in place:

```sh
slang-format -i rtl/
```

Configuration is discovered from `.slang/format.json`. See the
[command reference](docs/features/format-args.md),
[configuration reference](docs/features/configuration.md), and
[formatting philosophy](docs/features/format-philosophy.md).
See [validation](docs/features/format-validation.md) for output checks and force
behavior, and [formatting markers](docs/features/format-markers.md) for skipping
individual declarations or statements.

### pre-commit

Install `slang-format` on your `PATH`, then add this local hook to your
`.pre-commit-config.yaml`:

```yaml
repos:
  - repo: local
    hooks:
      - id: slang-format
        name: slang-format
        entry: slang-format
        language: system
        args: [-i]
        files: '\.(sv|svh|v|vh)$'
```

If you already have a `repos` list, append the hook's `repo: local` entry to it.
Enable the hook and format existing files with:

```sh
pre-commit install
pre-commit run slang-format --all-files
```

[prek](https://prek.j178.dev/) users can replace `pre-commit` with `prek` in
these commands. On commit, the hook formats staged Verilog and SystemVerilog
files in place, discovering `.slang/format.json` for each file. Review and stage
any formatting changes, then retry the commit. Files with input structural errors,
syntax-preservation failures, or idempotency failures are left unchanged without
failing the hook. Other errors, such as invalid configuration or file I/O errors,
still fail the hook. For a check-only hook that also fails on validation errors,
replace `args: [-i]` with `args: [--check]`.

See [DEVELOPING.md](DEVELOPING.md) for development setup and
[CONTRIBUTING.md](CONTRIBUTING.md) for contribution guidelines.

Licensed under the [MIT License](LICENSE).
