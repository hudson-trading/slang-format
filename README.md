# slang-format

`slang-format` is an opinionated SystemVerilog formatter built on top of the
[slang](https://github.com/MikePopoloski/slang) parser. It provides both a
standalone command-line tool and the `slang::format` CMake library target.

The formatter has its own version and release stream. Language servers and
other tools can pin this repository as a submodule and link it in-process,
while users can independently install a newer `slang-format` binary.

## Build

```sh
git clone --recurse-submodules https://github.com/hudson-trading/slang-format.git
cd slang-format
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
ctest --test-dir build --output-on-failure
```

The binary is written to `build/bin/slang-format`.

## Use

```sh
# Read stdin and write stdout
slang-format < input.sv

# Format files or directory trees in place
slang-format -i rtl/top.sv rtl/lib
```

Configuration is discovered from `.slang/format.json`. See the
[command reference](docs/features/format-args.md),
[configuration reference](docs/features/format-config.md), and
[formatting philosophy](docs/features/format-philosophy.md).
See [validation](docs/features/format-validation.md) for output checks and force
behavior, and [formatting markers](docs/features/format-markers.md) for skipping
individual declarations or statements.

## Embed

A parent project that already defines compatible `slang::slang` and
`reflectcpp::reflectcpp` targets can reuse them:

```cmake
set(SLANG_FORMAT_BUILD_CLI OFF)
set(SLANG_FORMAT_INCLUDE_TESTS OFF)
set(SLANG_FORMAT_INCLUDE_INSTALL OFF)
add_subdirectory(external/slang-format)
target_link_libraries(my_tool PRIVATE slang::format)
```

Otherwise, the formatter uses its vendored `slang` and `reflect-cpp`
submodules. Config discovery and JSON parsing are part of the same formatter
library, so embedded and standalone callers use identical behavior.

See [DEVELOPING.md](DEVELOPING.md) for development setup and
[CONTRIBUTING.md](CONTRIBUTING.md) for contribution guidelines.

Licensed under the [MIT License](LICENSE).
