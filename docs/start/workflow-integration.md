# Workflow Integration

After [installation](installing.md), you can run `slang-format` directly, add it
to a commit hook or CI job, or embed the formatter in another application.

## Format your project

Format all Verilog and SystemVerilog files in the directory in place:

```sh
slang-format -i rtl/
```

This collects `.sv`, `.svh`, `.v`, and `.vh` files. Each file uses the nearest
`.slang/format.json` found by searching its parent directories, with the current
directory as a fallback. Nested configs replace parent configs; omitted settings
use defaults.

To use the same settings across local commands, commit hooks, and CI, put
`.slang/format.json` at your repository root. For example:

```json
{
  "columnLimit": 100,
  "indentWidth": 4
}
```

Directory collection respects `excludeDirectoryNames`. To select particular
files or subdirectories when formatting the project root, set `projectPaths`
in that config and run `slang-format -i .`. See the
[configuration reference](../features/configuration.md) for details.

## pre-commit and prek

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
these commands. The hook uses your installed binary; it does not download or
build `slang-format`.

On commit, pre-commit passes matching staged filenames to `slang-format` in
batches. Each invocation can format multiple files, resolving `.slang/format.json`
separately for each one. Review and stage any formatting changes, then retry the
commit. Use the hook's `files` and `exclude` patterns to restrict which files
are passed; `projectPaths` controls directory traversal, not explicit filenames.

The hook omits `--strict`: input structural errors, syntax-preservation failures,
and idempotency failures leave the affected files unchanged without failing the
hook. Other errors, including invalid configuration, file I/O errors, merge
conflicts, failed reparsing, and internal errors, still fail it. See
[validation](../features/format-validation.md) for the full output and exit-status
policy.

## Check formatting in CI

Install the same formatter version used locally, then run:

```sh
slang-format --check rtl/
```

`--check` leaves files unchanged and exits with status 1 if a file needs formatting,
fails validation, or cannot be processed. Use this when CI should enforce both
formatting and validation. `--dry-run` previews formatting changes but does not
fail just because formatting would change a file.

To use the pre-commit hook in check-only mode, replace `args: [-i]` with
`args: [--check]` and run `pre-commit run slang-format --all-files` in CI.

## Use stdin in scripts

Pass source on stdin and read formatted source from stdout:

```sh
slang-format --assume-filename rtl/top.sv - < input.sv > formatted.sv
```

`--assume-filename` provides the path used for config discovery and diagnostics;
it does not read that file. Without it, stdin uses the current directory's config.
See the [command reference](../features/format-args.md) for more options.

## Embedding

Add slang-format as a subdirectory and link against the `slang::format` CMake
target to format SystemVerilog in-process:

```cmake
set(SLANG_FORMAT_BUILD_CLI OFF)
set(SLANG_FORMAT_INCLUDE_TESTS OFF)
set(SLANG_FORMAT_INCLUDE_INSTALL OFF)
add_subdirectory(external/slang-format)
target_link_libraries(my_tool PRIVATE slang::format)
```

A parent project that already defines compatible `slang::slang` and
`reflectcpp::reflectcpp` targets can reuse them. Otherwise, the formatter uses
its vendored `slang` and `reflect-cpp` submodules.

Config discovery and JSON parsing are part of the same formatter library, so
embedded and standalone callers use identical behavior. See
[configuration](../features/configuration.md) for the available settings.
