#!/usr/bin/env python3
"""Exercise formatter CLI contracts with isolated source and configuration files."""

import argparse
import csv
import json
import os
import stat
import queue
import threading
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

    def test_verbose_parallel_output(self):
        warned = self.root / "warned.sv"
        warned.write_bytes(b"// slang-format: on\n" + self.source)
        targets = [self.rejected, self.dirty, warned, self.clean]
        for mode in ["--dry-run", "--check", "-i"]:
            with self.subTest(mode=mode):
                serial = self.run_cli("--verbose", "-j1", mode, *targets)
                self.dirty.write_bytes(self.source)
                warned.write_bytes(b"// slang-format: on\n" + self.source)
                parallel = self.run_cli("--verbose", "-j4", mode, *targets)
                self.assertEqual(parallel.returncode, serial.returncode)
                self.assertEqual(parallel.stdout, b"")
                self.assertEqual(
                    parallel.stderr.splitlines()[-1], serial.stderr.splitlines()[-1]
                )
                for result in [serial, parallel]:
                    lines = [
                        line.rsplit(b" (", 1)[0]
                        if line.startswith(b"finished ")
                        else line
                        for line in result.stderr.splitlines()
                    ]
                    for target in targets:
                        start = f"formatting {target} ...".encode()
                        finish = f"finished {target}".encode()
                        self.assertEqual(lines.count(start), 1)
                        self.assertEqual(lines.count(finish), 1)
                        self.assertLess(lines.index(start), lines.index(finish))
                    self.assertIn(b"parse errors", result.stderr)
                    self.assertIn(b"slang-format: on", result.stderr)
        self.assertEqual(self.dirty.read_bytes(), self.clean.read_bytes())

    @unittest.skipIf(os.name == "nt", "requires POSIX named pipes")
    def test_verbose_reports_other_workers_while_first_file_stalls(self):
        stalled = self.root / "stalled.sv"
        os.mkfifo(stalled)
        # Keep the pipe open without supplying input, blocking the first worker's read.
        pipe = os.open(stalled, os.O_RDWR | os.O_NONBLOCK)
        report = self.root / "stats.csv"
        process = subprocess.Popen(
            [
                BINARY,
                "--verbose",
                "--stats-csv",
                str(report),
                "-j2",
                "--dry-run",
                str(stalled),
                str(self.clean),
            ],
            cwd=self.root,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
        )
        messages = queue.Queue()

        def read_stderr():
            for line in process.stderr:
                line = line.rstrip(b"\n")
                messages.put(
                    line.rsplit(b" (", 1)[0] if line.startswith(b"finished ") else line
                )

        reader = threading.Thread(target=read_stderr, daemon=True)
        reader.start()
        try:
            expected = {
                f"formatting {stalled} ...".encode(),
                f"formatting {self.clean} ...".encode(),
                f"finished {self.clean}".encode(),
            }
            observed = []
            while not expected.issubset(observed):
                try:
                    observed.append(messages.get(timeout=10))
                except queue.Empty:
                    self.fail(f"progress blocked behind stalled input: {observed}")
            self.assertNotIn(f"finished {stalled}".encode(), observed)
            self.assertIsNone(process.poll())
            with report.open(newline="") as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual([row["path"] for row in rows], [str(self.clean)])
            os.write(pipe, self.source)
            os.close(pipe)
            pipe = None
            self.assertEqual(process.wait(timeout=10), 0)
            reader.join(timeout=10)
            self.assertFalse(reader.is_alive())
            while not messages.empty():
                observed.append(messages.get_nowait())
            self.assertIn(f"finished {stalled}".encode(), observed)
        finally:
            if process.poll() is None:
                process.kill()
            process.wait(timeout=10)
            reader.join(timeout=10)
            process.stderr.close()
            if pipe is not None:
                os.close(pipe)

    def test_csv_timings_and_outcomes(self):
        report = self.root / "stats.csv"
        generated = self.root / "generated.sv"
        generated.write_bytes(b"// @generated\n" + self.source)
        quoted = self.root / (
            'quoted,"file\n.sv' if os.name != "nt" else "quoted,file.sv"
        )
        quoted.write_bytes(self.source)
        targets = [self.clean, self.dirty, self.rejected, generated, quoted]
        expected = ["unchanged", "changed", "skipped", "excluded", "changed"]
        for mode in ["--dry-run", "--check", "-i"]:
            with self.subTest(mode=mode):
                result = self.run_cli("--stats-csv", report, "-j4", mode, *targets)
                self.assertEqual(
                    result.returncode, 1 if mode == "--check" else 0, result.stderr
                )
                self.assertEqual(result.stdout, b"")
                self.assertNotIn(b"finished ", result.stderr)
                with report.open(newline="") as stream:
                    reader = csv.DictReader(stream)
                    self.assertEqual(
                        reader.fieldnames,
                        [
                            "path",
                            "elapsed_ms",
                            "input_bytes",
                            "status",
                            "validation_failed",
                        ],
                    )
                    rows = list(reader)
                self.assertEqual(len(rows), len(targets))
                by_path = {row["path"]: row for row in rows}
                for target, status in zip(targets, expected):
                    row = by_path[str(target)]
                    self.assertEqual(row["status"], status)
                    self.assertGreaterEqual(float(row["elapsed_ms"]), 0)
                    self.assertGreater(int(row["input_bytes"]), 0)
                    self.assertEqual(
                        row["validation_failed"],
                        "true" if target == self.rejected else "false",
                    )
        result = self.run_cli("--stats-csv", report, "--force", "-n", self.rejected)
        self.assertEqual(result.returncode, 1, result.stderr)
        with report.open(newline="") as stream:
            row = next(csv.DictReader(stream))
        self.assertEqual(row["status"], "changed")
        self.assertEqual(row["validation_failed"], "true")

    def test_csv_single_file_stdin_and_failures(self):
        report = self.root / "stats.csv"
        for options in [[], ["--dry-run"], ["--check"]]:
            for targets in [[], [self.dirty]]:
                result = self.run_cli(
                    "-v",
                    "--stats-csv",
                    report,
                    *options,
                    *targets,
                    source=self.source if not targets else None,
                )
                self.assertEqual(
                    result.returncode, 1 if "--check" in options else 0, result.stderr
                )
                self.assertEqual(
                    result.stdout, b"" if options else self.clean.read_bytes()
                )
                with report.open(newline="") as stream:
                    rows = list(csv.DictReader(stream))
                self.assertEqual(len(rows), 1)
                row = rows[0]
                self.assertEqual(
                    row["path"], "<stdin>" if not targets else str(self.dirty)
                )
                self.assertEqual(int(row["input_bytes"]), len(self.source))
                self.assertEqual(row["status"], "changed")
                timing = f"finished {row['path']} ({row['elapsed_ms']} ms)".encode()
                self.assertIn(timing, result.stderr)
        result = self.run_cli(
            "--stats-csv", report, "--assume-filename", "unsaved.sv", source=self.source
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        with report.open(newline="") as stream:
            self.assertEqual(next(csv.DictReader(stream))["path"], "unsaved.sv")
        empty = self.root / "empty"
        empty.mkdir()
        result = self.run_cli("--stats-csv", report, "-n", empty)
        self.assertEqual(result.returncode, 0, result.stderr)
        with report.open(newline="") as stream:
            self.assertEqual(list(csv.DictReader(stream)), [])
        for destination in [
            self.dirty,
            self.root,
            self.root / "missing/stats.csv",
            "-",
        ]:
            result = self.run_cli("--stats-csv", destination, "-i", self.dirty)
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertEqual(self.dirty.read_bytes(), self.source)
        config = self.root / "config.json"
        config.write_text("{}")
        result = self.run_cli("--config", config, "--stats-csv", config, self.dirty)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual(config.read_text(), "{}")
        self.dirty.chmod(0o444)
        try:
            result = self.run_cli("--stats-csv", report, "-i", self.dirty)
            self.assertEqual(result.returncode, 1, result.stderr)
            with report.open(newline="") as stream:
                self.assertEqual(next(csv.DictReader(stream))["status"], "error")
        finally:
            self.dirty.chmod(0o600)

    @unittest.skipIf(os.name == "nt", "symlink creation requires Windows privileges")
    def test_csv_rejects_source_aliases(self):
        for hardlink in [False, True]:
            alias = self.root / "alias.csv"
            if hardlink:
                os.link(self.dirty, alias)
            else:
                alias.symlink_to(self.dirty)
            try:
                result = self.run_cli("--stats-csv", alias, "-i", self.dirty)
                self.assertEqual(result.returncode, 1, result.stderr)
                self.assertEqual(self.dirty.read_bytes(), self.source)
            finally:
                alias.unlink()

    def test_csv_config_errors_and_layout(self):
        report = self.root / "stats.csv"
        bad = self.root / "bad"
        (bad / ".slang").mkdir(parents=True)
        (bad / ".slang/format.json").write_text('{"unknown": 1}')
        invalid = bad / "file.sv"
        invalid.write_bytes(self.source)
        result = self.run_cli(
            "--stats-csv", report, "--stage=layout", "-n", invalid, self.clean
        )
        self.assertEqual(result.returncode, 1, result.stderr)
        with report.open(newline="") as stream:
            rows = {row["path"]: row for row in csv.DictReader(stream)}
        self.assertEqual(rows[str(invalid)]["status"], "error")
        self.assertEqual(rows[str(invalid)]["input_bytes"], "0")
        self.assertEqual(rows[str(self.clean)]["status"], "unchanged")

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
            b"formatted 1 files, 1 unchanged, 1 excluded, "
            b"1 skipped (input parse errors), 0 failed",
            result.stderr,
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

    def test_summary_parse_skips_and_errors_are_exclusive(self):
        conflict = self.root / "conflict.sv"
        conflict.write_bytes(b"// heading\n<<<<<<< branch\n" + self.source)
        invalid = self.root / "bad/file.sv"
        config = self.root / "bad/.slang/format.json"
        config.parent.mkdir(parents=True)
        config.write_text('{"unknown": true}')
        invalid.write_bytes(self.source)
        original = self.rejected.read_bytes()
        for jobs in ["-j1", "-j4"]:
            for mode, status in [("--dry-run", 0), ("--check", 1), ("--strict", 1)]:
                with self.subTest(jobs=jobs, mode=mode):
                    args = [jobs, "--verbose", "-i", mode, self.clean, self.rejected]
                    result = self.run_cli(*args)
                    self.assertEqual(result.returncode, status, result.stderr)
                    prefix = b"formatted" if mode == "--strict" else b"would format"
                    self.assertEqual(
                        result.stderr.splitlines()[-1],
                        prefix + b" 0 files, 1 unchanged, 0 excluded, "
                        b"1 skipped (input parse errors), 0 failed",
                    )
                    result = self.run_cli(*args, conflict, invalid)
                    self.assertEqual(result.returncode, 1, result.stderr)
                    self.assertEqual(
                        result.stderr.splitlines()[-1],
                        prefix + b" 0 files, 1 unchanged, 0 excluded, "
                        b"1 skipped (input parse errors), 2 failed",
                    )
                    self.assertEqual(self.rejected.read_bytes(), original)
                    self.assertEqual(invalid.read_bytes(), self.source)

    def test_summary_forced_output_counts_only_its_outcome(self):
        original = self.rejected.read_bytes()
        result = self.run_cli("--force", self.rejected)
        self.assertEqual(result.returncode, 1, result.stderr)
        expected = result.stdout
        self.assertNotEqual(expected, self.rejected.read_bytes())
        result = self.run_cli("--force", "-i", self.rejected, self.clean)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual(self.rejected.read_bytes(), expected)
        self.assertEqual(
            result.stderr.splitlines()[-1],
            b"formatted 1 files, 1 unchanged, 0 excluded, 0 failed",
        )
        result = self.run_cli("--force", "-i", self.rejected, self.clean)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual(
            result.stderr.splitlines()[-1],
            b"formatted 0 files, 2 unchanged, 0 excluded, 0 failed",
        )
        self.rejected.write_bytes(original)
        self.rejected.chmod(0o444)
        try:
            result = self.run_cli("--force", "-i", self.rejected, self.clean)
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertEqual(
                result.stderr.splitlines()[-1],
                b"formatted 0 files, 1 unchanged, 0 excluded, 1 failed",
            )
            self.assertEqual(self.rejected.read_bytes(), original)
        finally:
            self.rejected.chmod(0o600)

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

    def test_inline_config(self):
        config = self.root / ".slang/format.json"
        config.parent.mkdir()
        config.write_text('{"indentWidth": 8, "columnLimit": 60}')
        options = '{"indentWidth": 2, "alignment": {"paddingLimit": 12}}'
        result = self.run_cli("--config-json", options, "--dump-config", self.dirty)
        self.assertEqual(result.returncode, 0, result.stderr)
        resolved = json.loads(result.stdout)
        self.assertEqual(resolved["indentWidth"], 2)
        self.assertEqual(resolved["columnLimit"], 100)
        self.assertEqual(
            resolved["alignment"], {"paddingLimit": 12, "groupSeparatorLines": 1}
        )
        override = self.root / "override.json"
        override.write_text(options)
        expected = self.run_cli("--config", override, self.dirty)
        self.assertEqual(expected.returncode, 0, expected.stderr)
        self.assertIn(b"\n  logic a;", expected.stdout)
        for args in [
            ("--config-json", options, self.dirty),
            ("--config-json=" + options, self.dirty),
            ("--config-json", options),
            ("--config-json", options, "--assume-filename", "unsaved.sv", "-"),
        ]:
            with self.subTest(args=args):
                result = self.run_cli(*args, source=self.source)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout, expected.stdout)
        config.write_text('{"invalidKey": true}')
        result = self.run_cli("--config-json", "{}", self.dirty)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, self.clean.read_bytes())

    def test_inline_config_rejects_invalid_options_before_writing(self):
        stats = self.root / "stats.csv"
        stats.write_bytes(b"existing statistics\n")
        for value, diagnostic in [
            ('{"indentWidht": 2}', b"indentWidht"),
            ('{"alignment": {"paddingLmit": 12}}', b"paddingLmit"),
            ('{"indentWidth": "two"}', b"indentWidth"),
            ('{"alignment": false}', b"alignment"),
            ("{", b"failed to parse"),
            ("[]", b"failed to parse"),
            ("null", b"failed to parse"),
        ]:
            for mode in [(), ("--dump-config",), ("-i", self.dirty, self.clean)]:
                with self.subTest(value=value, mode=mode):
                    result = self.run_cli(
                        "--config-json",
                        value,
                        "--stats-csv",
                        stats,
                        *mode,
                        source=self.source,
                    )
                    self.assertEqual(result.returncode, 1, result.stderr)
                    self.assertEqual(result.stdout, b"")
                    self.assertIn(b"--config-json", result.stderr)
                    self.assertIn(diagnostic, result.stderr)
                    self.assertEqual(self.dirty.read_bytes(), self.source)
                    self.assertEqual(stats.read_bytes(), b"existing statistics\n")
        for args in [
            ("--config", self.root / "missing.json", "--config-json", "{}"),
            ("--config-json", "{}", "--config", self.root / "missing.json"),
        ]:
            result = self.run_cli(*args, "-i", self.dirty)
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertIn(
                b"--config and --config-json cannot be combined", result.stderr
            )
            self.assertEqual(self.dirty.read_bytes(), self.source)

    def test_inline_config_directory_collection_and_parallel_files(self):
        project = self.root / "project"
        config = project / ".slang/format.json"
        config.parent.mkdir(parents=True)
        config.write_text('{"projectPaths": ["missing"], "indentWidth": 8}')
        kept = [project / "one.sv", project / "nested/two.sv"]
        skipped = project / "build/skipped.sv"
        for target in [*kept, skipped]:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(self.source)
        options = json.dumps(
            {
                "indentWidth": 2,
                "excludeDirectoryNames": ["build"],
                "projectPaths": ["missing"],
            }
        )
        result = self.run_cli("--config-json", options, "-i", "-j2", project)
        self.assertEqual(result.returncode, 0, result.stderr)
        for target in kept:
            self.assertIn(b"\n  logic a;", target.read_bytes())
        self.assertEqual(skipped.read_bytes(), self.source)

    def test_config_option_names(self):
        config = self.root / "format.json"
        options = {
            "alignment": {"paddingLimit": 12, "groupSeparatorLines": 3},
            "excludeDirectoryNames": ["build"],
            "projectPaths": ["src", "top.sv"],
        }
        config.write_text(json.dumps(options))
        result = self.run_cli("--config", config, "--dump-config")
        self.assertEqual(result.returncode, 0, result.stderr)
        resolved = json.loads(result.stdout)
        for key, value in options.items():
            self.assertEqual(resolved[key], value)
        self.assertNotIn("respectUserFormatting", resolved)
        for obsolete, value in [
            ("maxSpaces", {"alignment": {"maxSpaces": 12}}),
            ("linesBetweenGroups", {"alignment": {"linesBetweenGroups": 3}}),
            ("excludeDirs", {"excludeDirs": ["build"]}),
            ("dirs", {"dirs": ["src"]}),
            ("respectUserFormatting", {"respectUserFormatting": False}),
        ]:
            with self.subTest(obsolete=obsolete):
                config.write_text(json.dumps(value))
                result = self.run_cli("--config", config, self.dirty)
                self.assertEqual(result.returncode, 1, result.stderr)
                self.assertEqual(result.stdout, b"")
                self.assertIn(obsolete.encode(), result.stderr)

    def test_project_paths_and_excluded_directory_names(self):
        project = self.root / "project"
        config = project / ".slang/format.json"
        config.parent.mkdir(parents=True)
        config.write_text(
            json.dumps(
                {
                    "projectPaths": ["src", "top.sv"],
                    "excludeDirectoryNames": ["build"],
                }
            )
        )
        for path in [
            "top.sv",
            "src/keep.sv",
            "src/build/skipped.sv",
            "src/deep/build/nested.sv",
            "src/build_extra/included.sv",
            "other/file.sv",
        ]:
            target = project / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(self.source)
        selected = {"top.sv", "src/keep.sv", "src/build_extra/included.sv"}
        for args, expected in [
            ((project,), selected),
            ((project / "src/build",), {"src/build/skipped.sv"}),
            ((project / "src/deep/build/nested.sv",), {"src/deep/build/nested.sv"}),
            ((project / "other",), {"other/file.sv"}),
            (("--config", config, project), selected | {"other/file.sv"}),
        ]:
            with self.subTest(args=args):
                result = self.run_cli("--dry-run", "--verbose", *args)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(
                    {
                        line
                        for line in result.stderr.splitlines()
                        if line.startswith(b"formatting ")
                    },
                    {f"formatting {project / path} ...".encode() for path in expected},
                )

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
