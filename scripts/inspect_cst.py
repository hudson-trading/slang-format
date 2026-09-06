#!/usr/bin/env python3
"""Inspect CST trivia structure for SystemVerilog files.

Usage:
  python3 scripts/inspect_cst.py <file.sv> [--token TEXT] [--kind KIND] [--depth N]
  python3 scripts/inspect_cst.py <file.sv> --all-trivia
  python3 scripts/inspect_cst.py <file.sv> --directives

Examples:
  # Show all trivia on tokens matching "assign"
  python3 scripts/inspect_cst.py tests/format/ifdef_in_body.sv --token assign

  # Show all directive trivia in the file
  python3 scripts/inspect_cst.py tests/format/ifdef_with_comments.sv --directives

  # Show all non-whitespace trivia
  python3 scripts/inspect_cst.py tests/format/mixed_trivia.sv --all-trivia

  # Show disabled tokens for a specific directive kind
  python3 scripts/inspect_cst.py tests/format/ifdef_with_comments.sv --kind EndIfDirective
"""

import argparse
import json
import os
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_SLANG = os.path.join(SCRIPT_DIR, "..", "build", "bin", "slang")


def get_cst(path, slang):
    result = subprocess.run(
        [slang, "--parse-only", "--cst-json", "-", "--cst-json-mode", "full", path],
        capture_output=True,
        text=True,
    )
    # slang returns non-zero on parse errors (e.g. unknown macros), but still
    # produces valid CST JSON on stdout.  Only bail if there's no output at all.
    if not result.stdout.strip():
        print(result.stderr, file=sys.stderr)
        sys.exit(1)
    if result.stderr:
        print(result.stderr, file=sys.stderr, end="")
    return json.loads(result.stdout)


def fmt_trivia(t):
    """Format a single trivia entry for display."""
    kind = t.get("kind", "?")
    text = t.get("text", "")
    if kind == "Directive":
        s = t.get("syntax", {})
        sk = s.get("kind", "")
        expr = s.get("expr", {})
        name = ""
        if isinstance(expr, dict):
            n = expr.get("name", {})
            if isinstance(n, dict):
                name = n.get("text", "")
        dt_count = len(s.get("disabledTokens", []))
        d_tok = s.get("directive", {})
        d_trivia = d_tok.get("trivia", [])
        parts = [f"Directive({sk}"]
        if name:
            parts.append(f" {name}")
        parts.append(f", {dt_count} disabled tokens)")
        if d_trivia:
            parts.append("\n         directive trivia:")
            for dt in d_trivia:
                parts.append(
                    f"\n           {dt.get('kind')}: {repr(dt.get('text', '')[:60])}"
                )
        return "".join(parts)
    else:
        return f"{kind}: {repr(text[:80])}"


def print_token(tok, indent=0):
    """Print a token with its trivia."""
    prefix = "  " * indent
    kind = tok.get("kind", "?")
    text = tok.get("text", "")
    trivia = tok.get("trivia", [])
    print(f'{prefix}{kind}: "{text}"')
    for t in trivia:
        print(f"{prefix}  trivia: {fmt_trivia(t)}")


def walk_tokens(node, callback, path=""):
    """Walk all tokens in the CST and call callback(token, path) for each."""
    if isinstance(node, dict):
        # Check if this is a token (has "text" and "kind" but no children that are nodes)
        if "text" in node and "kind" in node:
            # Could be a token - check if it has trivia (tokens have trivia, syntax nodes don't)
            if "trivia" in node or not any(
                isinstance(v, (dict, list))
                for k, v in node.items()
                if k not in ("trivia",)
            ):
                callback(node, path)
                # Also walk into directive syntax within trivia
                for t in node.get("trivia", []):
                    if t.get("kind") == "Directive":
                        s = t.get("syntax", {})
                        walk_tokens(s, callback, f"{path}.trivia.{s.get('kind', '?')}")
                return

        for k, v in node.items():
            if k == "trivia":
                continue
            walk_tokens(v, callback, f"{path}.{k}")
    elif isinstance(node, list):
        for i, v in enumerate(node):
            walk_tokens(v, callback, f"{path}[{i}]")


def walk_directives(node, callback, path=""):
    """Walk all directive trivia entries and call callback(directive_syntax, path)."""
    if isinstance(node, dict):
        if "text" in node and "trivia" in node:
            for i, t in enumerate(node.get("trivia", [])):
                if t.get("kind") == "Directive":
                    s = t.get("syntax", {})
                    callback(s, f"{path}.trivia[{i}]", node)
                    # Recurse into disabled tokens
                    for j, dt in enumerate(s.get("disabledTokens", [])):
                        walk_directives(
                            dt, callback, f"{path}.trivia[{i}].disabled[{j}]"
                        )
            return
        for k, v in node.items():
            if k == "trivia":
                continue
            walk_directives(v, callback, f"{path}.{k}")
    elif isinstance(node, list):
        for i, v in enumerate(node):
            walk_directives(v, callback, f"{path}[{i}]")


def cmd_token(cst, text):
    """Show all trivia on tokens matching TEXT."""

    def cb(tok, path):
        if text.lower() in tok.get("text", "").lower():
            print(f"\n{path}")
            print_token(tok)

    walk_tokens(cst, cb)


def cmd_all_trivia(cst):
    """Show all tokens that have non-whitespace trivia."""

    def cb(tok, path):
        trivia = tok.get("trivia", [])
        interesting = [
            t for t in trivia if t.get("kind") not in ("Whitespace", "EndOfLine")
        ]
        if interesting:
            print(f"\n{path}")
            print_token(tok)

    walk_tokens(cst, cb)


def cmd_directives(cst):
    """Show all directive trivia with their structure."""

    def cb(syntax, path, parent_tok):
        sk = syntax.get("kind", "?")
        expr = syntax.get("expr", {})
        name = ""
        if isinstance(expr, dict):
            n = expr.get("name", {})
            if isinstance(n, dict):
                name = n.get("text", "")

        d_tok = syntax.get("directive", {})
        d_trivia = d_tok.get("trivia", [])

        label = f"{sk}"
        if name:
            label += f" ({name})"

        print(f"\n{path}: {label}")
        print(f'  on token: {parent_tok.get("kind")}: "{parent_tok.get("text")}"')

        if d_trivia:
            print("  directive trivia:")
            for t in d_trivia:
                print(f"    {t.get('kind')}: {repr(t.get('text', '')[:60])}")

        dt = syntax.get("disabledTokens", [])
        if dt:
            print(f"  disabled tokens ({len(dt)}):")
            for i, tok in enumerate(dt):
                trivia = tok.get("trivia", [])
                trivia_str = ""
                if trivia:
                    parts = []
                    for t in trivia:
                        parts.append(f"{t.get('kind')}({repr(t.get('text', '')[:30])})")
                    trivia_str = f"  trivia=[{', '.join(parts)}]"
                print(
                    f'    [{i:2d}] {tok.get("kind"):20s}: "{tok.get("text")}"{trivia_str}'
                )

    walk_directives(cst, cb)


def cmd_kind(cst, kind):
    """Show directive trivia matching a specific SyntaxKind."""

    def cb(syntax, path, parent_tok):
        if syntax.get("kind") == kind:
            print(f"\n{path}: {kind}")
            print(f'  on token: {parent_tok.get("kind")}: "{parent_tok.get("text")}"')

            d_tok = syntax.get("directive", {})
            d_trivia = d_tok.get("trivia", [])
            if d_trivia:
                print("  directive trivia:")
                for t in d_trivia:
                    print(f"    {t.get('kind')}: {repr(t.get('text', '')[:60])}")

            dt = syntax.get("disabledTokens", [])
            if dt:
                print(f"  disabled tokens ({len(dt)}):")
                for i, tok in enumerate(dt):
                    trivia = tok.get("trivia", [])
                    trivia_str = ""
                    if trivia:
                        parts = []
                        for t in trivia:
                            if t.get("kind") == "Directive":
                                s = t.get("syntax", {})
                                parts.append(f"Directive({s.get('kind', '')})")
                            else:
                                parts.append(
                                    f"{t.get('kind')}({repr(t.get('text', '')[:30])})"
                                )
                        trivia_str = f"  trivia=[{', '.join(parts)}]"
                    print(
                        f'    [{i:2d}] {tok.get("kind"):20s}: "{tok.get("text")}"{trivia_str}'
                    )

    walk_directives(cst, cb)


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("file", help="SystemVerilog file to inspect")
    parser.add_argument("--slang", default=DEFAULT_SLANG, help="Path to slang binary")

    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument(
        "--token", metavar="TEXT", help="Show trivia on tokens matching TEXT"
    )
    group.add_argument(
        "--all-trivia",
        action="store_true",
        help="Show all tokens with non-whitespace trivia",
    )
    group.add_argument(
        "--directives",
        action="store_true",
        help="Show all directive trivia with structure",
    )
    group.add_argument(
        "--kind",
        metavar="KIND",
        help="Show directives matching SyntaxKind (e.g. EndIfDirective)",
    )

    args = parser.parse_args()
    cst = get_cst(args.file, args.slang)

    if args.token:
        cmd_token(cst, args.token)
    elif args.all_trivia:
        cmd_all_trivia(cst)
    elif args.directives:
        cmd_directives(cst)
    elif args.kind:
        cmd_kind(cst, args.kind)


if __name__ == "__main__":
    main()
