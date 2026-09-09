#!/usr/bin/env python3
"""Test runner for slang-format.

Runs both independently renderable formatter stages for every .sv test case,
compares against stage-specific goldens, and verifies CST equivalence.

Usage:
  test_format.py [--update] [--slang-format PATH] [--slang PATH] [FILE ...]

Test layout:
  tests/format/
    basic.sv              # input test case
    basic.layout.out.sv   # pre-alignment golden
    basic.out.sv          # aligned golden
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from diff_cst import get_cst_json

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.normpath(os.path.join(SCRIPT_DIR, ".."))
TESTS_DIR = os.path.join(PROJECT_DIR, "tests", "format")

sys.path.insert(0, SCRIPT_DIR)


def _get_diff_cmd() -> list[str] | None:
    """Return a command list for opening a diff, or None if unavailable.

    Uses $EDITOR to determine the diff tool. Only returns a command if
    stdout is a TTY (interactive terminal) and the editor is found on PATH.
    Handles $EDITOR values with arguments (e.g. 'code -w').
    """
    if not sys.stdout.isatty():
        return None
    editor = os.environ.get("EDITOR", "")
    if not editor:
        return None
    parts = editor.split()
    binary = parts[0]
    args = parts[1:]
    basename = os.path.basename(binary)
    if "code" in basename:
        # VS Code / code-insiders / codium
        if shutil.which(binary):
            return [binary, *args, "--diff"]
    elif basename in ("vim", "nvim", "vi", "vimdiff", "nvimdiff"):
        tool = "nvimdiff" if "nvim" in basename else "vimdiff"
        if shutil.which(tool):
            return [tool]
    return None


def open_diff(left: str, right: str) -> None:
    """Open a diff between two files in the user's editor, or print a command."""
    cmd = _get_diff_cmd()
    if cmd:
        subprocess.Popen(
            [*cmd, left, right],
            stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    else:
        print(f"    diff {left} {right}")


def find_test_cases(tests_dir: str) -> list[str]:
    """Find all .sv test cases (files with exactly one dot in the basename).

    Walks subdirectories so tests in e.g. tests/format/prop/ are included.
    Returns paths relative to tests_dir (e.g. 'basic.sv', 'prop/foo.sv').
    """
    cases = []
    for dirpath, _dirnames, filenames in os.walk(tests_dir):
        for f in sorted(filenames):
            if f.endswith(".sv") and f.count(".") == 1:
                relpath = os.path.relpath(os.path.join(dirpath, f), tests_dir)
                cases.append(relpath)
    return sorted(cases)


def run_formatter(
    path: str, slang_format: str, stage: str = "aligned"
) -> tuple[str, str, int]:
    """Run slang-format on a file and return (stdout, stderr, returncode)."""
    result = subprocess.run(
        [slang_format, "--force", "--stage", stage, path],
        capture_output=True,
    )
    return result.stdout.decode(), result.stderr.decode(), result.returncode


def check_stage(
    *,
    case: str,
    input_path: str,
    stem: str,
    stage: str,
    golden_path: str,
    actual_path: str,
    slang_format: str,
    slang: str,
    have_slang: bool,
    update: bool,
) -> bool:
    """Run and validate one independently renderable formatter stage."""
    label = "layout" if stage == "layout" else "aligned"
    formatted, stderr, rc = run_formatter(input_path, slang_format, stage)

    if rc < 0:
        print(f"  CRASH    {case} [{label}]: slang-format killed by signal {-rc}")
        if stderr:
            print(f"           {stderr.strip()}", file=sys.stderr)
        return False

    # Recovery fixtures deliberately contain parse errors. --force emits their
    # output with failure status; still check goldens and preservation below.
    forced_parse_error = (
        rc == 1 and "cannot reliably format source with parse errors" in stderr
    )
    if rc != 0 and not forced_parse_error:
        print(f"  FAIL     {case} [{label}]: slang-format exited with code {rc}")
        if stderr:
            print(f"           {stderr.strip()}", file=sys.stderr)
        if formatted:
            with open(actual_path, "w") as f:
                f.write(formatted)
            open_diff(
                golden_path if os.path.exists(golden_path) else input_path, actual_path
            )
        return False

    warning_lines = [
        line for line in stderr.strip().splitlines() if "dropped token" in line
    ]
    if warning_lines:
        print(f"  FAIL     {case} [{label}]: dropped-token warnings on stderr")
        for line in warning_lines[:10]:
            print(f"           {line}")
        if len(warning_lines) > 10:
            print(f"           ... and {len(warning_lines) - 10} more")
        return False

    if "formatting is not idempotent" in stderr:
        print(f"  FAIL     {case} [{label}]: formatter output is not idempotent")
        for line in stderr.strip().splitlines():
            print(f"           {line}")
        return False

    if "formatting changed the syntax tree" in stderr:
        print(f"  CST MISMATCH  {case} [{label}]: formatting changed the syntax tree")
        for line in stderr.strip().splitlines():
            print(f"           {line}")
        with open(actual_path, "w") as f:
            f.write(formatted)
        open_diff(input_path, actual_path)
        return False

    if update:
        already_matches = False
        if os.path.exists(golden_path):
            with open(golden_path) as f:
                already_matches = f.read() == formatted
        if already_matches:
            print(f"  PASS     {case} [{label}]")
        else:
            with open(golden_path, "w") as f:
                f.write(formatted)
            print(f"  UPDATED  {case} [{label}]")
    else:
        if not os.path.exists(golden_path):
            print(
                f"  FAIL     {case} [{label}]: golden file missing (run with --update)"
            )
            return False
        with open(golden_path) as f:
            expected = f.read()
        if formatted != expected:
            print(f"  FAIL     {case} [{label}]: output differs from golden")
            with open(actual_path, "w") as f:
                f.write(formatted)
            open_diff(golden_path, actual_path)
            return False
        print(f"  PASS     {case} [{label}]")

    if not have_slang:
        return True

    with tempfile.NamedTemporaryFile(mode="wb", suffix=".sv", delete=False) as tmp:
        tmp.write(formatted.encode())
        tmp_path = tmp.name
    try:
        orig_json = get_cst_json(input_path, slang, "no-whitespace", quiet=True)
        fmt_json = get_cst_json(tmp_path, slang, "no-whitespace", quiet=True)
        try:
            orig_json = json.dumps(json.loads(orig_json), indent=2) + "\n"
            fmt_json = json.dumps(json.loads(fmt_json), indent=2) + "\n"
        except json.JSONDecodeError:
            pass
        if orig_json == fmt_json:
            return True

        suffix = ".layout" if stage == "layout" else ""
        orig_cst_path = os.path.join(TESTS_DIR, f"{stem}.cst.json")
        fmt_cst_path = os.path.join(TESTS_DIR, f"{stem}{suffix}.out.cst.json")
        with open(orig_cst_path, "w") as f:
            f.write(orig_json)
        with open(fmt_cst_path, "w") as f:
            f.write(fmt_json)
        print(f"UNDETECTED CST MISMATCH  {case} [{label}]")
        open_diff(input_path, golden_path)
        open_diff(orig_cst_path, fmt_cst_path)
        return False
    finally:
        os.unlink(tmp_path)


def main():
    parser = argparse.ArgumentParser(description="Test runner for slang-format")
    parser.add_argument(
        "--update", action="store_true", help="Update golden output files"
    )
    parser.add_argument(
        "--cst",
        action="store_true",
        help="Open a diff of the full CST JSON (with trivia) before/after formatting for a single test",
    )
    parser.add_argument(
        "--build",
        action="store_true",
        help="Build slang-format before running tests",
    )
    parser.add_argument(
        "--slang-format",
        default=os.path.join(SCRIPT_DIR, "..", "build", "bin", "slang-format"),
        help="Path to slang-format binary (default: build/bin/slang-format)",
    )
    parser.add_argument(
        "--slang",
        default=os.path.join(PROJECT_DIR, "build", "bin", "slang"),
        help="Path to slang binary for CST comparison (default: build/bin/slang)",
    )
    parser.add_argument(
        "--exclude-prop",
        action="store_true",
        help="Skip ignored proprietary smoke tests",
    )
    parser.add_argument(
        "filter",
        nargs="*",
        help="File paths or substrings to filter test cases (direct paths supported with --cst)",
    )
    args = parser.parse_args()

    if args.build:
        print("Building slang-format...")
        result = subprocess.run(
            [
                "cmake",
                "--build",
                os.path.join(PROJECT_DIR, "build"),
                "-j8",
                "--target",
                "slang_format",
            ],
            cwd=PROJECT_DIR,
        )
        if result.returncode != 0:
            print("error: build failed", file=sys.stderr)
            sys.exit(1)

    # Check if the slang binary is available for CST equivalence checks
    have_slang = os.path.isfile(args.slang) and os.access(args.slang, os.X_OK)
    if not have_slang:
        print(
            f"  NOTE: slang binary not found at {args.slang}, "
            "skipping external CST equivalence checks"
        )

    if args.cst:
        if not have_slang:
            print("error: --cst requires the slang binary", file=sys.stderr)
            sys.exit(1)
        # Resolve the single target: either a direct file path or a test filter
        if args.filter and len(args.filter) == 1 and os.path.isfile(args.filter[0]):
            input_path = os.path.abspath(args.filter[0])
            stem = input_path.removesuffix(".sv")
        else:
            if not os.path.isdir(TESTS_DIR):
                print(f"error: test directory not found: {TESTS_DIR}", file=sys.stderr)
                sys.exit(1)
            cases = find_test_cases(TESTS_DIR)
            if args.filter:
                cases = [c for c in cases if any(f in c for f in args.filter)]
            if len(cases) != 1:
                print(
                    "error: --cst requires exactly one test (use filter)",
                    file=sys.stderr,
                )
                sys.exit(1)
            input_path = os.path.join(TESTS_DIR, cases[0])
            stem = os.path.join(TESTS_DIR, cases[0].removesuffix(".sv"))
        golden_path = f"{stem}.out.sv"

        formatted, stderr, rc = run_formatter(input_path, args.slang_format)
        with tempfile.NamedTemporaryFile(mode="wb", suffix=".sv", delete=False) as tmp:
            tmp.write(formatted.encode())
            tmp_path = tmp.name

        try:
            orig_cst_path = f"{stem}.cstfull.json"
            fmt_cst_path = f"{stem}.out.cstfull.json"

            # Full CST diff (with whitespace/trivia) — shows formatting changes
            orig_json = get_cst_json(input_path, args.slang, "full", quiet=True)
            fmt_json = get_cst_json(tmp_path, args.slang, "full", quiet=True)
            try:
                orig_json = json.dumps(json.loads(orig_json), indent=2) + "\n"
                fmt_json = json.dumps(json.loads(fmt_json), indent=2) + "\n"
            except json.JSONDecodeError:
                pass
            with open(orig_cst_path, "w") as f:
                f.write(orig_json)
            with open(fmt_cst_path, "w") as f:
                f.write(fmt_json)

            open_diff(orig_cst_path, fmt_cst_path)
        finally:
            os.unlink(tmp_path)
        return

    if not os.path.isdir(TESTS_DIR):
        print(f"error: test directory not found: {TESTS_DIR}", file=sys.stderr)
        sys.exit(1)

    cases = find_test_cases(TESTS_DIR)
    if args.exclude_prop:
        cases = [case for case in cases if not case.startswith("prop/")]
    if args.filter:
        cases = [c for c in cases if any(f in c for f in args.filter)]
    if not cases:
        print(f"No test cases found in {TESTS_DIR}", file=sys.stderr)
        sys.exit(1)

    failures = 0

    for case in cases:
        input_path = os.path.join(TESTS_DIR, case)
        stem = case.removesuffix(".sv")
        stages = (
            (
                "layout",
                os.path.join(TESTS_DIR, f"{stem}.layout.out.sv"),
                os.path.join(TESTS_DIR, f"{stem}.layout.actual.sv"),
            ),
            (
                "aligned",
                os.path.join(TESTS_DIR, f"{stem}.out.sv"),
                os.path.join(TESTS_DIR, f"{stem}.actual.sv"),
            ),
        )
        case_ok = True
        for stage, golden_path, actual_path in stages:
            case_ok &= check_stage(
                case=case,
                input_path=input_path,
                stem=stem,
                stage=stage,
                golden_path=golden_path,
                actual_path=actual_path,
                slang_format=args.slang_format,
                slang=args.slang,
                have_slang=have_slang,
                update=args.update,
            )
        if not case_ok:
            failures += 1

    if not args.update:
        print()
        passed = len(cases) - failures
        print(f"{passed}/{len(cases)} tests passed")
        if failures:
            sys.exit(1)


if __name__ == "__main__":
    main()
