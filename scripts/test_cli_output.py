#!/usr/bin/env python3
"""Check lossless CLI output for exclusions and rejected formatting."""

import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--slang-format", required=True)
    args = parser.parse_args()
    binary = str(Path(args.slang_format).resolve())
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        config = root / "config.json"
        config.write_text("{}")
        command = [binary, "--config", str(config)]
        sources = {
            "generated": b"// @generated\r\nmodule foo; endmodule",
            "rejected": (
                b"module foo;\r\nnested_in_if: always @(posedge clk) "
                b"assert property (a |-> b); endmodule"
            ),
        }
        checks = 0
        for name, source in sources.items():
            path = root / f"{name}.sv"
            path.write_bytes(source)
            for stage in ("layout", "aligned"):
                for stdin in (False, True):
                    invocation = command + ["--stage", stage]
                    if not stdin:
                        invocation.append(str(path))
                    result = subprocess.run(
                        invocation, input=source if stdin else None, capture_output=True
                    )
                    assert result.returncode == 0, result.stderr
                    assert result.stdout == source, (name, stage, stdin, result)
                    dry_run = subprocess.run(
                        invocation + ["--dry-run"],
                        input=source if stdin else None,
                        capture_output=True,
                    )
                    assert dry_run.returncode == 0, dry_run.stderr
                    assert dry_run.stdout == b""
                    if name == "rejected":
                        assert b"parse errors" in result.stderr
                        forced = subprocess.run(
                            invocation + ["--force"],
                            input=source if stdin else None,
                            capture_output=True,
                        )
                        assert forced.returncode == 1, forced.stderr
                        assert forced.stdout and forced.stdout != source
                    checks += 1
                inplace = subprocess.run(
                    command + ["--stage", stage, "-i", str(path)], capture_output=True
                )
                assert inplace.returncode == 0, inplace.stderr
                assert path.read_bytes() == source
                checks += 1
        print(f"{checks} CLI output checks passed")


if __name__ == "__main__":
    main()
