#!/usr/bin/env python3
"""Exercise formatter limits and a configuration matrix in isolated processes."""

import argparse
import json
import os
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--slang-format", required=True)
    args = parser.parse_args()
    configs = [
        {"columnLimit": 40},
        {"indentWidth": 2},
        {"indentWidth": 0},
        {"columnLimit": 0},
        {"spacesBeforeTrailingComment": 0},
    ]
    failures = []
    count = 0
    fixtures = Path(__file__).resolve().parent.parent / "tests" / "format"
    for source in sorted(fixtures.glob("*.sv")):
        if source.name.count(".") != 1:
            continue
        for config in configs:
            result = subprocess.run(
                [
                    args.slang_format,
                    "--dry-run",
                    "--strict",
                    "--config-json",
                    json.dumps(config),
                    str(source),
                ],
                capture_output=True,
                text=True,
                timeout=30,
            )
            count += 1
            # Deliberately malformed recovery fixtures may fail input preflight,
            # but no configuration may crash or introduce output validation errors.
            if result.returncode and not (
                result.returncode == 1
                and "cannot reliably format source with parse errors" in result.stderr
                and "not idempotent" not in result.stderr
                and "changed the syntax tree" not in result.stderr
                and "internal error" not in result.stderr
            ):
                failures.append(f"{source.name} {config}: {result.stderr}")

    def limit_stack():
        import resource

        _, hard = resource.getrlimit(resource.RLIMIT_STACK)
        resource.setrlimit(
            resource.RLIMIT_STACK,
            (min(8 * 1024 * 1024, hard) if hard >= 0 else 8 * 1024 * 1024, hard),
        )

    for operands in (2040, 30000):
        source = (
            "module foo; assign value = "
            + "+".join(["signal_a"] * operands)
            + "; endmodule\n"
        )
        result = subprocess.run(
            [args.slang_format, "--strict", "--config-json", '{"columnLimit":0}'],
            input=source,
            capture_output=True,
            text=True,
            timeout=30,
            preexec_fn=limit_stack if os.name == "posix" else None,
        )
        if operands == 2040:
            if result.returncode:
                failures.append(f"supported chain failed: {result.stderr}")
        elif result.returncode != 1 or "depth limit" not in result.stderr:
            failures.append(
                f"deep chain failed unsafely: exit {result.returncode}, {result.stderr}"
            )
    if failures:
        raise SystemExit("\n".join(failures))
    print(f"{count} configuration checks and 2 bounded-stack stress checks passed")


if __name__ == "__main__":
    main()
