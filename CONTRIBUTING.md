# Contributing

Issues and pull requests are welcome.

See [DEVELOPING.md](DEVELOPING.md) for the source architecture, test workflow,
generated files, and embedding instructions.

Before submitting a change:

1. Add or update a focused formatter test in `tests/format/`.
2. Run `python3 scripts/test_format.py --build`.
3. Run `ctest --test-dir build --output-on-failure`.
4. Run `prek run --all-files`.

Keep commits focused and do not include proprietary source in regression
tests. Reduce real-world failures to small examples with generic identifiers.

By contributing, you agree that your contributions will be licensed under the
project's [MIT License](LICENSE).
