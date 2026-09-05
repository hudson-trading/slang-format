# slang-format development guide

`slang-format` is an opinionated SystemVerilog formatter built on the vendored
slang parser in `external/slang`.

## Build and test

```sh
cmake -B build
cmake --build build -j8
ctest --test-dir build --output-on-failure
python3 scripts/test_format.py --build
```

The main outputs are `build/bin/slang-format`, the `slang_format_lib` target,
and its `slang::format` alias.

Read `src/format/CLAUDE.md` before changing formatter internals. Develop
formatting behavior in focused `tests/format/*.sv` cases and review generated
goldens before keeping them. Never commit proprietary inputs from
`tests/format/prop/`.

Run `python3 scripts/genconfig.py` after changing
`include/format/FormatConfig.h`.
