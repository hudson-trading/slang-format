# Contributing

Issues and pull requests are welcome.

See [DEVELOPING.md](DEVELOPING.md) for the source architecture, test tooling,
generated files, and embedding instructions.

Instead of immediately adding a style change, please raise an issue to discuss whether it should be
a formatter change or a config-gated change.

Submit formatter behavior changes as two stacked pull requests, or two commits in a PR.

1. The first change adds a focused input under `tests/format/` and the
   generated `.layout.out.sv` and `.out.sv` goldens that record the formatter's
   current behavior. It should not contain the formatter fix.
2. The second change contains the formatter fix
   and updates the goldens to the intended output. Keep the test input unchanged
   so the golden diff shows the behavior change directly.

Generate or update goldens with:

```sh
python3 scripts/test_format.py --build --update name_filter
```

Before submitting each pull request, run:

```sh
python3 scripts/test_format.py --build
ctest --test-dir build --output-on-failure
prek run --all-files
```

Keep commits focused and do not include proprietary source in regression
tests. Reduce real-world failures to small examples with generic identifiers.

By contributing, you agree that your contributions will be licensed under the
project's [MIT License](LICENSE).
