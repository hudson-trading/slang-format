# Installing

<!-- Uncomment after the first release is published.
Prebuilt archives are available on the
[GitHub releases page](https://github.com/hudson-trading/slang-format/releases).
-->

## Build from source

Requirements:

- A C++20 compiler
- CMake 3.23 or newer
- Git
- Python 3 for code generation and the formatter test harness

```sh
git clone --recurse-submodules https://github.com/hudson-trading/slang-format.git
cd slang-format
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
ctest --test-dir build --output-on-failure
```

## Install

The binary is `build/bin/slang-format`. Install it with:

```sh
cmake --install build --prefix /desired/prefix
```

Add `/desired/prefix/bin` to `PATH`, or copy the binary to a directory already on
`PATH`.

Verify the installation:

```sh
slang-format --version
```

The standalone formatter is versioned independently from slang-server. An
editor may use its own embedded formatter revision while command-line and CI
workflows use a separately installed release.
