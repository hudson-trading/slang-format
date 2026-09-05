# Building

Requirements:

- A C++20 compiler
- CMake 3.23 or newer
- Git
- Python 3 for the formatter test harness

```sh
git clone --recurse-submodules https://github.com/hudson-trading/slang-format.git
cd slang-format
cmake -B build
cmake --build build -j8
ctest --test-dir build --output-on-failure
```

The binary is `build/bin/slang-format`. Install it with:

```sh
cmake --install build --prefix /desired/prefix
```
