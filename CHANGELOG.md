# Changelog

This project follows [Semantic Versioning](https://semver.org/). Formatter
releases are independent from slang-server releases.

## Unreleased

- Extract the formatter library, CLI, configuration, tests, and release
  tooling into the standalone `slang-format` repository.
- Expose the `slang::format` CMake target for in-process embedding.
- Report formatter-owned version metadata from `slang-format --version`.

## 0.1.0

- Initial standalone release.
