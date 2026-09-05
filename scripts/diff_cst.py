#!/usr/bin/env python3
"""Diff the CST-JSON of two SystemVerilog files.

Usage: diff_cst.py <file1.sv> <file2.sv> [--mode MODE] [--slang PATH]

Modes (from slang --cst-json-mode):
  full           - include all trivia (default)
  simple-trivia  - simplify trivia representation
  no-whitespace      - strip all trivia (whitespace, comments, etc.)
  simple-tokens  - simplify token representation
"""

import argparse
import difflib
import json
import os
import subprocess
import sys


def get_cst_json(path: str, slang: str, mode: str, quiet: bool = False) -> str:
    result = subprocess.run(
        [
            slang,
            "--parse-only",
            "--cst-json",
            "-",
            "--cst-json-mode",
            mode,
            "--cst-json-no-expand",
            path,
        ],
        capture_output=True,
        text=True,
    )
    if result.stderr and not quiet:
        print(result.stderr, end="", file=sys.stderr)
    return result.stdout


def diff_cst_json(
    file1: str,
    file2: str,
    slang: str,
    mode: str = "no-whitespace",
    context: int = 3,
    quiet: bool = False,
) -> list[str]:
    """Compare CST JSON of two files, returning unified diff lines.

    Returns an empty list if the CSTs are equivalent.
    """
    json1 = get_cst_json(file1, slang, mode, quiet=quiet)
    json2 = get_cst_json(file2, slang, mode, quiet=quiet)

    try:
        json1 = json.dumps(json.loads(json1), indent=2) + "\n"
        json2 = json.dumps(json.loads(json2), indent=2) + "\n"
    except json.JSONDecodeError as e:
        return [f"Failed to parse CST JSON: {e}\n"]

    lines1 = json1.splitlines(keepends=True)
    lines2 = json2.splitlines(keepends=True)

    return list(
        difflib.unified_diff(lines1, lines2, fromfile=file1, tofile=file2, n=context)
    )


def main():
    parser = argparse.ArgumentParser(
        description="Diff the CST-JSON of two SystemVerilog files"
    )
    parser.add_argument("file1", help="First SystemVerilog file")
    parser.add_argument("file2", help="Second SystemVerilog file")
    parser.add_argument(
        "--mode",
        default="no-whitespace",
        choices=["full", "simple-trivia", "no-whitespace", "simple-tokens"],
        help="CST JSON mode (default: no-whitespace)",
    )
    script_dir = os.path.dirname(os.path.abspath(__file__))
    default_slang = os.path.join(script_dir, "..", "build", "bin", "slang")
    parser.add_argument("--slang", default=default_slang, help="Path to slang binary")
    parser.add_argument(
        "--context", "-C", type=int, default=3, help="Number of context lines in diff"
    )
    args = parser.parse_args()

    diff = diff_cst_json(args.file1, args.file2, args.slang, args.mode, args.context)

    if diff:
        sys.stdout.writelines(diff)
        sys.exit(1)


if __name__ == "__main__":
    main()
