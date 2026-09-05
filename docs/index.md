# slang-format

`slang-format` is an opinionated SystemVerilog formatter built on the slang
parser. It is available as a standalone binary and as the `slang::format`
CMake target for applications that want to format in-process.

The formatter intentionally has its own versions and releases. A language
server can pin a tested formatter revision, while users of the command-line
tool can upgrade independently.

Start with:

- [Installing](start/installing.md)
- [Building from source](start/building.md)
- [Formatting philosophy](features/format-philosophy.md)
- [Command reference](features/format-args.md)
- [Configuration reference](features/format-config.md)
