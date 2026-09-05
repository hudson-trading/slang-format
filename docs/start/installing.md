# Installing

Download a prebuilt archive from the
[GitHub releases page](https://github.com/hudson-trading/slang-format/releases)
or [build from source](building.md). Put `slang-format` somewhere on `PATH`.

Verify the installation:

```sh
slang-format --version
```

The standalone formatter is versioned independently from slang-server. An
editor may use its own embedded formatter revision while command-line and CI
workflows use a separately installed release.
