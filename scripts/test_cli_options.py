#!/usr/bin/env python3
"""Exercise formatter CLI contracts with isolated source and configuration files."""

import argparse
from pathlib import Path
import subprocess
import tempfile
import unittest


class CliOptionsTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.source = b"module foo;logic a;endmodule\n"
        self.dirty = self.root / "dirty.sv"
        self.dirty.write_bytes(self.source)
        self.clean = self.root / "clean.sv"
        self.clean.write_bytes(self.run_cli(self.dirty).stdout)
        self.rejected = self.root / "rejected.sv"
        self.rejected.write_bytes(
            b"module foo;\nnested_in_if: always @(posedge clk) "
            b"assert property (a |-> b); endmodule\n"
        )

    def run_cli(self, *args, source=None):
        return subprocess.run(
            [BINARY, *map(str, args)],
            cwd=self.root,
            input=source,
            capture_output=True,
            timeout=30,
        )

    def test_check_modes(self):
        for flags in [("--check",), ("--verify",), ("-n", "--Werror")]:
            for targets, expected in [
                ([self.clean], 0),
                ([self.dirty], 1),
                ([self.dirty, self.clean], 1),
                ([self.clean, self.dirty], 1),
                ([self.rejected], 1),
                ([self.root / "missing.sv"], 1),
            ]:
                with self.subTest(flags=flags, targets=targets):
                    result = self.run_cli(*flags, *targets)
                    self.assertEqual(result.returncode, expected, result.stderr)
                    self.assertEqual(result.stdout, b"")
            result = self.run_cli(*flags, source=self.source)
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertEqual(result.stdout, b"")
            result = self.run_cli(*flags, "-i", "--force", self.dirty)
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertEqual(self.dirty.read_bytes(), self.source)
            self.assertEqual(result.stdout, b"")
            result = self.run_cli(*flags, "--force", self.rejected)
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertEqual(result.stdout, b"")

    def test_check_exclusions_and_warnings(self):
        for prefix in [b"// @generated\n", b"// slang-format: off\n"]:
            result = self.run_cli("--check", source=prefix + self.source)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout, b"")
        warned = b"// slang-format: on\n" + self.clean.read_bytes()
        result = self.run_cli("--check", source=warned)
        self.assertEqual(result.returncode, 0, result.stderr)
        result = self.run_cli("-n", "--Werror", source=warned)
        self.assertEqual(result.returncode, 1, result.stderr)

    def test_dry_run_reports_changes_without_failing(self):
        result = self.run_cli("--dry-run", self.dirty)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, b"")
        self.assertIn(b"needs formatting", result.stderr)
        self.assertEqual(self.dirty.read_bytes(), self.source)
        result = self.run_cli("--dry-run", self.clean)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn(b"needs formatting", result.stderr)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--slang-format", required=True)
    args, remaining = parser.parse_known_args()
    BINARY = str(Path(args.slang_format).resolve())
    unittest.main(argv=[__file__, *remaining])
