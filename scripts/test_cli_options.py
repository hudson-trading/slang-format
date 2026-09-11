#!/usr/bin/env python3
"""Exercise formatter CLI contracts with isolated source and configuration files."""

import argparse
import os
import stat
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

    def test_inplace_writes_and_counts(self):
        timestamp = 1_600_000_000_000_000_000
        os.utime(self.clean, ns=(timestamp, timestamp))
        before = self.clean.stat()
        generated = self.root / "generated.sv"
        generated.write_bytes(b"// @generated\n" + self.source)
        result = self.run_cli("-i", self.clean, self.dirty, generated, self.rejected)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.clean.stat().st_mtime_ns, before.st_mtime_ns)
        self.assertEqual(self.clean.stat().st_ino, before.st_ino)
        self.assertIn(
            b"formatted 1 files, 1 unchanged, 1 excluded, 1 failed", result.stderr
        )
        self.assertEqual(self.dirty.read_bytes(), self.clean.read_bytes())
        self.assertEqual(list(self.root.glob(".slang-format-*.tmp")), [])
        self.dirty.write_bytes(self.source)
        self.dirty.chmod(0o640)
        result = self.run_cli("-i", self.dirty)
        self.assertEqual(result.returncode, 0, result.stderr)
        if os.name != "nt":
            self.assertEqual(stat.S_IMODE(self.dirty.stat().st_mode), 0o640)
        self.dirty.write_bytes(self.source)
        self.dirty.chmod(0o444)
        try:
            result = self.run_cli("-i", self.dirty)
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertEqual(self.dirty.read_bytes(), self.source)
            self.assertEqual(list(self.root.glob(".slang-format-*.tmp")), [])
        finally:
            self.dirty.chmod(0o600)

    @unittest.skipIf(os.name == "nt", "symlink creation requires Windows privileges")
    def test_inplace_preserves_symlink(self):
        link = self.root / "link.sv"
        link.symlink_to(self.dirty.name)
        result = self.run_cli("-i", link)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(link.is_symlink())
        self.assertEqual(self.dirty.read_bytes(), self.clean.read_bytes())

    def test_per_file_configs(self):
        for name, width in [("one", 2), ("two", 8)]:
            folder = self.root / name
            (folder / ".slang").mkdir(parents=True)
            (folder / ".slang" / "format.json").write_text(
                '{"indentWidth": %d}' % width
            )
            (folder / "file.sv").write_bytes(self.source)
        one, two = self.root / "one/file.sv", self.root / "two/file.sv"
        for targets in [(one, two), (two, one), (self.root,)]:
            one.write_bytes(self.source)
            two.write_bytes(self.source)
            result = self.run_cli("-i", *targets)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn(b"\n  logic a;", one.read_bytes())
            self.assertIn(b"\n        logic a;", two.read_bytes())
        override = self.root / "override.json"
        override.write_text('{"indentWidth": 6}')
        result = self.run_cli("-i", "--config", override, one, two)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(one.read_bytes(), two.read_bytes())
        self.assertIn(b"\n      logic a;", one.read_bytes())
        for contents in ['{"indentWidht": 8}', '{"alignment": {"maxSpace": 3}}']:
            override.write_text(contents)
            result = self.run_cli("--config", override, self.dirty)
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertEqual(result.stdout, b"")
        (self.root / "one/.slang/format.json").write_text('{"indentWidht": 8}')
        two.write_bytes(self.source)
        result = self.run_cli("-i", one, two)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn(b"\n        logic a;", two.read_bytes())

    def test_stdin_paths(self):
        expected = self.clean.read_bytes()
        result = self.run_cli("-", source=self.source)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, expected)
        for args in [("-i", "-"), ("-", self.dirty), ("-", "-")]:
            result = self.run_cli(*args, source=self.source)
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertEqual(result.stdout, b"")
        project = self.root / "project"
        (project / ".slang").mkdir(parents=True)
        (project / ".slang" / "format.json").write_text('{"indentWidth": 8}')
        for option in ["--assume-filename", "--stdin_name"]:
            for inputs in [(), ("-",)]:
                result = self.run_cli(
                    option, "project/unsaved.sv", *inputs, source=self.source
                )
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn(b"        logic a;", result.stdout)
                result = self.run_cli(
                    option,
                    "project/unsaved.sv",
                    "--strict",
                    *inputs,
                    source=self.rejected.read_bytes(),
                )
                self.assertEqual(result.returncode, 1, result.stderr)
                self.assertIn(b"project/unsaved.sv:2:", result.stderr)
        result = self.run_cli("--assume-filename", "project/unsaved.sv", self.dirty)
        self.assertEqual(result.stdout, expected)
        override = self.root / "override.json"
        override.write_text('{"indentWidth": 2}')
        result = self.run_cli(
            "--assume-filename",
            "project/unsaved.sv",
            "--config",
            override,
            source=self.source,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(b"\n  logic a;", result.stdout)
        result = self.run_cli("--check", "-", source=self.source)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual(result.stdout, b"")

    def test_strict_validation_preserves_rejected_output(self):
        original = self.rejected.read_bytes()
        for flags in [
            ("--strict",),
            ("--fail-on-incomplete-format",),
            ("--failsafe_success=false",),
        ]:
            for mode in [(), ("-n",), ("-i",)]:
                with self.subTest(flags=flags, mode=mode):
                    result = self.run_cli(*flags, *mode, self.rejected)
                    self.assertEqual(result.returncode, 1, result.stderr)
                    self.assertEqual(result.stdout, b"" if mode else original)
                    self.assertEqual(self.rejected.read_bytes(), original)
            for targets in [(self.clean, self.rejected), (self.rejected, self.clean)]:
                result = self.run_cli(*flags, "-n", *targets)
                self.assertEqual(result.returncode, 1, result.stderr)
            result = self.run_cli(*flags, self.dirty)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout, self.clean.read_bytes())
            result = self.run_cli(*flags, source=original)
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertEqual(result.stdout, original)
        result = self.run_cli("--failsafe_success=true", self.rejected)
        self.assertEqual(result.returncode, 0, result.stderr)

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
