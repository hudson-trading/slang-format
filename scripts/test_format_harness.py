#!/usr/bin/env python3
"""Regression checks for formatter harness failures and safe golden updates."""

import contextlib
import io
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import diff_cst
import test_format


class HarnessTests(unittest.TestCase):
    def test_checker_must_produce_a_tree(self):
        for status, output in [
            (0, ""),
            (1, ""),
            (0, "null"),
            (0, "{}"),
            (0, '{"syntaxTrees":[]}'),
            (0, '{"syntaxTrees":[{"error":"failed"}]}'),
            (0, '{"error":"failed"}'),
            (-11, '{"syntaxTrees":[{"kind":"CompilationUnit"}]}'),
        ]:
            with self.subTest(status=status, output=output):
                result = subprocess.CompletedProcess(
                    [], status, output, "checker failure"
                )
                with patch("diff_cst.subprocess.run", return_value=result):
                    with self.assertRaises(RuntimeError):
                        diff_cst.get_cst_json(
                            "input.sv", "slang", "no-whitespace", quiet=True
                        )

    def test_recovered_parse_errors_can_produce_valid_json(self):
        result = subprocess.CompletedProcess(
            [],
            1,
            '{"syntaxTrees":[{"kind":"SyntaxTree","root":{"kind":"CompilationUnit"}}]}',
            "parse error",
        )
        with patch("diff_cst.subprocess.run", return_value=result):
            self.assertEqual(
                diff_cst.get_cst_json("input.sv", "slang", "full", quiet=True),
                result.stdout,
            )

    def test_failed_validation_never_updates_golden(self):
        with tempfile.TemporaryDirectory() as directory:
            golden = Path(directory) / "test.out.sv"
            golden.write_text("original golden")
            for failure in [
                RuntimeError("checker failed"),
                subprocess.TimeoutExpired("slang", 60),
            ]:
                with (
                    patch(
                        "test_format.run_formatter", return_value=("candidate", "", 0)
                    ),
                    patch("test_format.get_cst_json", side_effect=failure),
                    contextlib.redirect_stdout(io.StringIO()),
                ):
                    self.assertFalse(
                        test_format.check_stage(
                            case="test.sv",
                            input_path="test.sv",
                            stem="test",
                            stage="layout",
                            golden_path=str(golden),
                            actual_path=str(Path(directory) / "actual.sv"),
                            slang_format="formatter",
                            slang="slang",
                            have_slang=True,
                            update=True,
                        )
                    )
                self.assertEqual(golden.read_text(), "original golden")

    def test_update_reports_failure_status(self):
        with (
            patch("sys.argv", ["test_format.py", "--update"]),
            patch("test_format.find_test_cases", return_value=["test.sv"]),
            patch("test_format.check_stage", return_value=False),
            contextlib.redirect_stdout(io.StringIO()),
        ):
            with self.assertRaises(SystemExit) as failure:
                test_format.main()
            self.assertEqual(failure.exception.code, 1)


if __name__ == "__main__":
    unittest.main()
