# Developing slang-format

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

For formatter golden tests during iteration:

```sh
python3 scripts/test_format.py --build
python3 scripts/test_format.py --build name_filter
python3 scripts/test_format.py --build --update name_filter
```

Review updated golden files before keeping them. The harness checks layout and
aligned output, CST equivalence, and idempotency.

When `include/format/FormatConfig.h` changes, regenerate the schema and
documentation:

```sh
python3 scripts/genconfig.py
```

Run repository checks before requesting review:

```sh
prek run --all-files
```

The public embedding target is `slang::format`. Standalone-only targets are
disabled by default when this repository is included with `add_subdirectory`,
so a parent project can supply its own compatible `slang::slang` and
`reflectcpp::reflectcpp` targets without building duplicate dependencies.
