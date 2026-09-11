# Developing slang-format

## Configure, build, and test

Initialize the dependencies and configure the project:

```sh
git submodule update --init --recursive
cmake -B build
```

Build and run the complete test suite:

```sh
cmake --build build -j8
ctest --test-dir build --output-on-failure
```

The main outputs are `build/bin/slang-format`, the `slang_format_lib` CMake
target, and its public `slang::format` alias.

## Python environment and documentation

Use one project environment for development tools and documentation:

```sh
uv sync --locked
source .venv/bin/activate
mkdocs serve
```

The formatter test scripts and code generators use only the Python standard
library. The project dependencies provide MkDocs, its Material theme, and HTML
link validation. Local checks and CI use `prek`, installed separately:

```sh
uv tool install prek
prek run --all-files
```

It reads `.pre-commit-config.yaml` and caches each hook's tools separately from
the project environment. Run `prek install` to run the hooks automatically on
each commit.

Docs deployment and PR previews are disabled by job-level `if: ${{ false }}`
guards in their workflows. Remove those guards to enable publishing.

When enabled, docs deployments use `scripts/ci/publish-docs.sh` to update `gh-pages`. Main
deployments preserve `previews/`; PR deployments replace only their own
`previews/pr-N` directory. Concurrent pushes retry against the latest branch
without force-pushing. The `gh-pages` branch must already exist.

Manual preview runs require a PR number and publish the selected ref without
posting a PR comment.

## Architecture

The formatter uses the concrete syntax tree (CST) from slang so that comments,
directives, disabled preprocessor branches, and other source trivia remain
available throughout formatting. The core pipeline has three steps:

1. `src/NormalizedFormat.cpp` converts the slang CST into a lossless,
   formatter-owned tree. It classifies trivia, for example attaching trailing comments to the preceding token, multiline comments as their own nodes, and represents conditional preprocessor branches explicitly.
2. `src/Layout.cpp` lowers the normalized tree into the document IR declared
   in `include/format/FormatDocument.h`. This is where syntax-specific spacing,
   indentation, wrapping, and alignment anchors are chosen.
3. `src/FormatDocument.cpp` solves soft line breaks and renders the document.
   The layout stage chooses line breaks first; the aligned stage may add breaks
   and column padding but must not remove a layout-stage break.

`src/Formatter.cpp` connects these steps through the public `Formatter` API.
`src/FormatValidation.cpp` parses input, runs the formatter, and checks the
result for CST equivalence and idempotency. The lower-level comparisons live in
`src/CstValidation.cpp`.

Other important files are:

- `src/FormatStyle.cpp` for token-spacing and syntax-list policy.
- `src/FormatConfig.cpp` for configuration loading and discovery.
- `src/formatter_main.cpp` for the command-line interface and directory walk.
- `include/format/` for the embeddable public API.

For character classification, prefer the helpers in
`external/slang/include/slang/text/CharInfo.h` over hand-written character
tests or the locale-sensitive `<cctype>` functions.

The formatter intentionally does not expand macros. Macro invocations and
preprocessor directives retain their original source text from CST trivia, except
for indentation before arguments and closing parentheses in multiline invocations.
Whitespace within an argument is preserved because it can affect stringification.

## Formatter tests

Most behavior is covered by source and golden files under `tests/format/`.
Each input is rendered through both independently testable stages:

```text
tests/format/example.sv
tests/format/example.layout.out.sv
tests/format/example.out.sv
```

Run the whole golden suite, a filtered subset, or update a filtered golden:

```sh
scripts/test_format.py --build
scripts/test_format.py --build name_filter
scripts/test_format.py --build --update name_filter
```

Review every changed golden before keeping it. The harness checks expected
output, CST equivalence, and idempotency for both stages.

Formatting changes should have a focused test named after the behavior being
exercised. Include a contrasting case when it clarifies where the rule should
and should not apply. Real-world failures should be reduced to small examples
with generic identifiers; proprietary source must not be committed.

For CST and trivia debugging, use the repository helpers rather than invoking
slang's CST JSON mode directly:

```sh
scripts/test_format.py --cst name_filter
scripts/inspect_cst.py tests/format/example.sv --directives
scripts/inspect_cst.py tests/format/example.sv --token assign
```

The C++ unit tests under `tests/cpp/` cover configuration, normalization, and
validation independently of the golden suite.

## Configuration and generated files

The configuration types are declared in `include/format/FormatConfig.h`.
`slang-format` discovers `.slang/format.json` by walking upward from each
file independently, with the current working directory as a fallback. An explicit
`--config` path or `--config-json` object takes precedence for all files; these
options cannot be combined. Both use the same parser and reject unknown keys.
Directory collection uses each directory argument's config; collected files can
have their own nested formatting configs.

After changing the configuration types, regenerate the JSON schema and user
documentation:

```sh
scripts/genconfig.py
```

`src/FormatStyleGen.inc` is generated from slang's syntax definitions and the
list policies in `scripts/gen_format_style.py`. Every syntax list must have an
explicit body, vertical, dynamic, or inline policy; generation fails when a
list is missing or classified more than once. Check or update it with:

```sh
scripts/gen_format_style.py --check
scripts/gen_format_style.py --write
```

## Embedding

A parent CMake project can add this repository as a subdirectory and link
against `slang::format`:

```cmake
set(SLANG_FORMAT_BUILD_CLI OFF)
set(SLANG_FORMAT_INCLUDE_TESTS OFF)
set(SLANG_FORMAT_INCLUDE_INSTALL OFF)
add_subdirectory(external/slang-format)
target_link_libraries(my_tool PRIVATE slang::format)
```

When compatible `slang::slang` and `reflectcpp::reflectcpp` targets already
exist, slang-format reuses them. Otherwise it adds its vendored submodules.
Configuration loading and discovery are part of the formatter library, so
embedded and standalone callers share the same behavior.

`format::format()` returns a `FormatResult` whose `diagnostics` collection owns
each diagnostic's kind, message, and optional input line. Use `isUsable()`
to check validation and `outputAction(force)` to choose formatted output,
unchanged input, or an abort. Force overrides validation diagnostics except depth-limit
skips, which always preserve the input. Formatting otherwise produces a candidate when diagnostics are present;
callers must check the output action before applying it.
`generated` identifies files skipped because of an `@generated` comment;
`parseErrorCount` separately counts parser errors, which can be safely recovered
without producing a validation failure.
Unmatched `on` markers are reported as warnings; `isUsable()` and
`outputAction()` allow output when these are the only diagnostics.
An unmatched `off` silently preserves the rest of its list scope.

See [validation](docs/features/format-validation.md) for CLI output and exit status behavior.
