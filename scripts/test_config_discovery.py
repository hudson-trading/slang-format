#!/usr/bin/env python3
"""Integration test for slang-format config discovery and `dirs` scoping.

Verifies that when no --config is given, slang-format finds .slang/format.json
by walking up from the *target* path (the file/dir being formatted), not from
the current working directory. An explicit --config still overrides, and stdin
mode falls back to CWD-based discovery.

Also verifies the config's `dirs`: when the formatter is pointed at the config
root (the directory holding .slang/format.json), formatting is scoped to those
subtrees; pointing it at any other directory ignores `dirs`.

Each case builds a throwaway directory tree under a temp dir and runs the
binary, checking either the effective columnLimit (via --dump-config) or the
set of files it would format (via --dry-run -v).

Usage:
  test_config_discovery.py [--slang-format PATH]
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import tempfile

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.normpath(os.path.join(SCRIPT_DIR, ".."))
DEFAULT_BIN = os.path.join(PROJECT_DIR, "build", "bin", "slang-format")


def dump_column_limit(
    binary: str, args: list[str], cwd: str, stdin: str | None = None
) -> int:
    """Run slang-format --dump-config and return the effective columnLimit."""
    result = subprocess.run(
        [binary, "--dump-config", *args],
        cwd=cwd,
        input=stdin,
        capture_output=True,
        text=True,
    )
    cfg = json.loads(result.stdout)
    return cfg["columnLimit"]


def formatted_files(binary: str, args: list[str], cwd: str) -> set[str]:
    """Run slang-format --dry-run -v and return the set of basenames it
    reports it would format. Verbose prints 'formatting <path> ...' per file;
    a single-file run prints the formatted source to stdout instead, so this
    is only meaningful for multi-file (directory) targets."""
    result = subprocess.run(
        [binary, "--dry-run", "-v", *args],
        cwd=cwd,
        capture_output=True,
        text=True,
    )
    names: set[str] = set()
    for line in (result.stdout + result.stderr).splitlines():
        line = line.strip()
        if line.startswith("formatting ") and line.endswith("..."):
            path = line[len("formatting ") : -len(" ...")].strip()
            names.add(os.path.basename(path))
    return names


def main() -> None:
    parser = argparse.ArgumentParser(description="Test slang-format config discovery")
    parser.add_argument(
        "--slang-format", default=DEFAULT_BIN, help="Path to slang-format binary"
    )
    args = parser.parse_args()
    binary = args.slang_format

    failures = 0

    def check(name: str, got, want) -> None:
        nonlocal failures
        if got == want:
            print(f"  PASS     {name} ({got})")
        else:
            print(f"  FAIL     {name}: got {got}, expected {want}")
            failures += 1

    with tempfile.TemporaryDirectory() as tmp:
        # Project tree with a config at proj/.slang/format.json (columnLimit 40).
        proj = os.path.join(tmp, "proj")
        os.makedirs(os.path.join(proj, ".slang"))
        os.makedirs(os.path.join(proj, "src", "sub"))
        with open(os.path.join(proj, ".slang", "format.json"), "w") as f:
            f.write('{"columnLimit": 40}\n')
        target_file = os.path.join(proj, "src", "sub", "foo.sv")
        with open(target_file, "w") as f:
            f.write("module m; endmodule\n")

        # A CWD that has NO config above it.
        elsewhere = os.path.join(tmp, "elsewhere")
        os.makedirs(elsewhere)
        bare = os.path.join(elsewhere, "bare.sv")
        with open(bare, "w") as f:
            f.write("module m; endmodule\n")

        override = os.path.join(tmp, "override.json")
        with open(override, "w") as f:
            f.write('{"columnLimit": 55}\n')

        # 1. Target a file under proj, from an unrelated CWD -> discovers proj config.
        check(
            "target file walks up to config",
            dump_column_limit(binary, [target_file], cwd=elsewhere),
            40,
        )

        # 2. Target a directory under proj, from an unrelated CWD -> discovers config.
        check(
            "target dir walks up to config",
            dump_column_limit(binary, [os.path.join(proj, "src")], cwd=elsewhere),
            40,
        )

        # 3. Target a file with no config in its tree -> default (100).
        check(
            "no config in tree -> default",
            dump_column_limit(binary, [bare], cwd=elsewhere),
            100,
        )

        # 4. Explicit --config overrides discovery.
        check(
            "--config overrides discovery",
            dump_column_limit(
                binary, ["--config", override, target_file], cwd=elsewhere
            ),
            55,
        )

        # 5. stdin mode falls back to CWD-based discovery.
        check(
            "stdin uses cwd discovery",
            dump_column_limit(
                binary, [], cwd=os.path.join(proj, "src"), stdin="module m; endmodule\n"
            ),
            40,
        )

        # 5b. Target file outside any config tree, but CWD is under a config
        #     root -> falls back to CWD discovery. This is the lint-runner case:
        #     the tool formats a throwaway tempfile (in /tmp, no config above
        #     it) while running inside the project, so only CWD reflects the
        #     real config. `bare` lives under `elsewhere/` (no config); run it
        #     with cwd under `proj/` and expect proj's columnLimit (40).
        check(
            "target without config falls back to cwd",
            dump_column_limit(binary, [bare], cwd=os.path.join(proj, "src")),
            40,
        )

        # 5c. When BOTH the target tree and CWD have a config, the target tree
        #     wins (target-based discovery is tried first; CWD is only a
        #     fallback). `target_file` is under proj (columnLimit 40); run with
        #     cwd under a *different* config root (columnLimit 55 via override-
        #     style tree) and expect proj's 40.
        cwdrepo = os.path.join(tmp, "cwdrepo")
        os.makedirs(os.path.join(cwdrepo, ".slang"))
        os.makedirs(os.path.join(cwdrepo, "work"))
        with open(os.path.join(cwdrepo, ".slang", "format.json"), "w") as f:
            f.write('{"columnLimit": 55}\n')
        check(
            "target config wins over cwd config",
            dump_column_limit(binary, [target_file], cwd=os.path.join(cwdrepo, "work")),
            40,
        )

        # --- `dirs` scoping ----------------------------------------------
        # Config root with `dirs: ["fpga"]`. fpga/ is in scope; other/ is not.
        droot = os.path.join(tmp, "droot")
        os.makedirs(os.path.join(droot, ".slang"))
        os.makedirs(os.path.join(droot, "fpga", "src"))
        os.makedirs(os.path.join(droot, "other", "src"))
        with open(os.path.join(droot, ".slang", "format.json"), "w") as f:
            f.write('{"dirs": ["fpga"]}\n')
        # Two files per subtree so a directory run is always multi-file (a
        # single-file run prints to stdout instead of the verbose listing).
        for rel in (
            "fpga/src/a.sv",
            "fpga/src/a2.sv",
            "other/src/b.sv",
            "other/src/b2.sv",
        ):
            with open(os.path.join(droot, rel), "w") as f:
                f.write("module m;\nendmodule\n")

        # 6. Targeting the config root scopes to `dirs` (only fpga files).
        check(
            "dirs scopes config-root target",
            formatted_files(binary, [droot], cwd=tmp),
            {"a.sv", "a2.sv"},
        )

        # 7. Targeting a subfolder directly ignores `dirs` (ad-hoc format).
        check(
            "dirs ignored for ad-hoc subfolder",
            formatted_files(binary, [os.path.join(droot, "other")], cwd=tmp),
            {"b.sv", "b2.sv"},
        )

        # 8. Config root reached via trailing slash still applies `dirs`.
        check(
            "dirs applies with trailing slash",
            formatted_files(binary, [droot + os.sep], cwd=tmp),
            {"a.sv", "a2.sv"},
        )

        # 9. Without `dirs`, targeting the config root formats everything.
        with open(os.path.join(droot, ".slang", "format.json"), "w") as f:
            f.write("{}\n")
        check(
            "no dirs -> format whole tree",
            formatted_files(binary, [droot], cwd=tmp),
            {"a.sv", "a2.sv", "b.sv", "b2.sv"},
        )

    print()
    total = 11
    print(f"{total - failures}/{total} config-discovery tests passed")
    if failures:
        sys.exit(1)


if __name__ == "__main__":
    main()
