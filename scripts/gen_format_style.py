#!/usr/bin/env python3
"""Generate FormatStyle functions from slang's syntax definitions.

Reads syntax.txt to determine:
  1. Which (SyntaxKind, listIndex) pairs correspond to list fields that
     should be formatted vertically (getListStyle).
  2. Which SyntaxKinds have non-standard attribute lists
     (isNonstandardAttributeListSyntax).

Usage:
  python3 scripts/gen_format_style.py [--check | --write]

Without flags, prints the generated code to stdout.
"""

import argparse
import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SYNTAX_TXT = os.path.join(
    SCRIPT_DIR, "..", "external", "slang", "scripts", "syntax.txt"
)
OUTPUT_FILE = os.path.join(SCRIPT_DIR, "..", "src", "format", "FormatStyleGen.inc")

# Body list configuration: always vertical, even when the list is empty.
# These are undelimited member/item lists (module bodies, class bodies, etc.)
# where a newline is desired between the header and closing keyword.
BODY_LISTS = [
    ("AnonymousProgram", "members"),
    ("CheckerDeclaration", "members"),
    ("ClassDeclaration", "items"),
    ("ClockingDeclaration", "items"),
    ("CompilationUnit", "members"),
    ("CovergroupDeclaration", "members"),
    ("FunctionDeclaration", "items"),
    ("GenerateBlock", "members"),
    ("GenerateRegion", "members"),
    ("InterfaceDeclaration", "members"),
    ("LibraryMap", "members"),
    ("ModuleDeclaration", "members"),
    ("PackageDeclaration", "members"),
    ("ProgramDeclaration", "members"),
    ("SpecifyBlock", "items"),
    ("TaskDeclaration", "items"),
]

# Vertical list configuration: always one-item-per-line when non-empty.
# These are delimited lists (port lists, enum members, etc.) that stay
# inline when empty.
VERTICAL_LISTS = [
    ("AnsiPortList", "ports"),
    ("AnsiUdpPortList", "ports"),
    ("AssertionItemPortList", "ports"),
    ("BlockStatement", "items"),
    ("CaseGenerate", "items"),
    ("CaseStatement", "items"),
    ("ConfigDeclaration", "localparams"),
    ("ConfigDeclaration", "rules"),
    ("ConstraintBlock", "items"),
    ("CoverCross", "members"),
    ("Coverpoint", "members"),
    ("EnumType", "members"),
    ("InterfaceHeader", "imports"),
    ("ModuleHeader", "imports"),
    ("NonAnsiPortList", "ports"),
    ("NonAnsiUdpPortList", "ports"),
    ("PackageHeader", "imports"),
    ("ParameterPortList", "declarations"),
    ("Production", "rules"),
    ("ProgramHeader", "imports"),
    ("RandCaseStatement", "items"),
    ("StructType", "members"),
    ("StructuredAssignmentPattern", "items"),
    ("UnionType", "members"),
]

# Dynamic list configuration: vertical when the list has comments or exceeds
# the column limit, otherwise inline.
DYNAMIC_LISTS = [
    ("ArgumentList", "parameters"),
    ("ConcatenationExpression", "expressions"),
    ("FunctionPortList", "ports"),
    ("HierarchicalInstance", "connections"),
    ("ParameterValueAssignment", "parameters"),
    ("RangeList", "valueRanges"),
    ("SimpleAssignmentPattern", "items"),
]


def parse_syntax(path):
    """Parse syntax.txt and return (types, kindmap).

    types: dict of type_name -> {
        'base': base_type_name,
        'fields': [(type_str, field_name, is_list, list_kind), ...],
        'multikind': bool,
    }
    kindmap: dict of kind_name -> type_name
    """
    types = {"SyntaxNode": {"base": None, "fields": [], "multikind": False}}
    kindmap = {}

    currkind = None
    currtype_name = None
    tags = None
    currfields = None

    with open(path) as f:
        for line in f:
            line = line.rstrip("\n").strip()
            if line.startswith("//"):
                continue
            elif not line or (currfields is not None and line == "empty"):
                if currfields is not None:
                    _finalize_type(currtype_name, tags, currfields, types, kindmap)
                currtype_name = None
                currkind = None
                currfields = None
                tags = None
            elif currfields is not None:
                parts = line.split(" ")
                if len(parts) == 2:
                    currfields.append(parts)
            elif currkind is not None:
                for k in line.split(" "):
                    if k:
                        kindmap[k] = currkind
            elif line.startswith("kindmap<"):
                currkind = line[8 : line.index(">")] + "Syntax"
            else:
                parts = line.split(" ")
                currtype_name = parts[0] + "Syntax"
                tags = {}
                for t in parts[1:]:
                    k, _, v = t.partition("=")
                    tags[k] = v
                currfields = []

    if currfields is not None:
        _finalize_type(currtype_name, tags, currfields, types, kindmap)

    return types, kindmap


def _finalize_type(name, tags, fields, types, kindmap):
    base = tags.get("base", "") + "Syntax" if "base" in tags else "SyntaxNode"
    multikind = tags.get("multiKind") == "true"

    processed = []
    for ftype, fname in fields:
        if ftype.startswith("list<") or ftype.startswith("separated_list<"):
            is_list = True
            list_kind = (
                "SyntaxList" if ftype.startswith("list<") else "SeparatedSyntaxList"
            )
        else:
            is_list = False
            list_kind = None
        processed.append((ftype, fname, is_list, list_kind))

    types[name] = {
        "base": base,
        "fields": processed,
        "multikind": multikind,
    }

    is_final = tags.get("final", "true") != "false"
    if is_final and not multikind:
        kind_name = name[:-6] if name.endswith("Syntax") else name
        if kind_name not in kindmap:
            kindmap[kind_name] = name


def get_combined_fields(type_name, types):
    """Get all fields including inherited ones, with their child indices."""
    chain = []
    t = type_name
    while t and t != "SyntaxNode":
        if t not in types:
            break
        chain.append(t)
        t = types[t]["base"]

    fields = []
    for t in reversed(chain):
        fields.extend(types[t]["fields"])
    return fields


def get_kinds_for_type(type_name, kindmap):
    """Get all SyntaxKind values that map to this type."""
    kinds = []
    for kind_name, mapped_type in kindmap.items():
        if mapped_type == type_name:
            kinds.append(kind_name)
    return sorted(kinds)


def resolve_list_entries(entries, style, types, kindmap):
    """Resolve (KindName, fieldName) entries to (kind, listIndex, fieldName,
    typeName, numLists, style) tuples.

    KindName is a SyntaxKind value or multiKind type name, resolved to its
    parent type via the kindmap.
    """
    results = []

    for kind_name, field_name in entries:
        # Resolve kind name to type name via kindmap, or fall back to
        # treating it as a type name directly.
        if kind_name in kindmap:
            type_name = kindmap[kind_name]
        else:
            type_name = kind_name + "Syntax"

        if type_name not in types:
            print(f"Warning: {kind_name} not found in syntax.txt", file=sys.stderr)
            continue

        combined = get_combined_fields(type_name, types)
        num_lists = sum(1 for _, _, is_list, _ in combined if is_list)

        # Get all SyntaxKind values for this type. For multiKind types
        # (e.g. BlockStatement -> Sequential/Parallel), this expands to
        # all concrete kinds. If the entry itself is a specific kind
        # within a multiKind type, only emit that one kind.
        kinds = get_kinds_for_type(type_name, kindmap)
        if kind_name in kinds:
            kinds = [kind_name]
        elif not kinds:
            kinds = [kind_name]

        found = False
        list_idx = 0
        for _child_idx, (ftype, fname, is_list, list_kind) in enumerate(combined):
            if not is_list:
                continue
            if fname == field_name and is_list:
                for kind in kinds:
                    results.append((kind, list_idx, fname, type_name, num_lists, style))
                found = True
                break
            list_idx += 1

        if not found:
            print(
                f"Warning: field '{field_name}' not found as a list in {type_name}",
                file=sys.stderr,
            )

    return results


def find_list_styles(types, kindmap):
    """Find all (kind, listIndex, fieldName, typeName, numLists, style) tuples."""
    results = resolve_list_entries(BODY_LISTS, "Body", types, kindmap)
    results += resolve_list_entries(VERTICAL_LISTS, "Vertical", types, kindmap)
    results += resolve_list_entries(DYNAMIC_LISTS, "Dynamic", types, kindmap)
    return sorted(results, key=lambda x: x[0])


def inherits_from(type_name, ancestor, types):
    """Check if type_name inherits from ancestor."""
    t = type_name
    while t and t != "SyntaxNode":
        if t == ancestor:
            return True
        if t not in types:
            break
        t = types[t]["base"]
    return False


def find_attribute_lists(types, kindmap):
    """Find kinds with non-standard attribute lists.

    Excludes MemberSyntax (attributes at child 0) and StatementSyntax
    (attributes at child 1) descendants, since those are detected at
    runtime via isKind().
    """
    results = []

    for type_name in types:
        if type_name == "SyntaxNode":
            continue
        # Skip Member, Statement, and Expression hierarchies:
        # Member/Statement attributes are detected via isKind() at runtime.
        # Expression attributes are handled by formatExpression().
        if inherits_from(type_name, "MemberSyntax", types):
            continue
        if inherits_from(type_name, "StatementSyntax", types):
            continue
        if inherits_from(type_name, "ExpressionSyntax", types):
            continue

        combined = get_combined_fields(type_name, types)
        kinds = get_kinds_for_type(type_name, kindmap)
        if not kinds:
            continue

        for _child_idx, (ftype, fname, is_list, list_kind) in enumerate(combined):
            if ftype == "list<AttributeInstance>" and fname == "attributes":
                for kind in kinds:
                    results.append(kind)

    return sorted(results, key=lambda x: x[0])


def generate_vertical_list_function(results):
    """Generate getListStyle() implementation."""
    lines = [
        "ListStyle getListStyle(SyntaxKind parentKind, size_t listIndex) {",
        "    switch (parentKind) {",
    ]

    by_kind = {}
    for kind, idx, fname, tname, num_lists, style in results:
        by_kind.setdefault(kind, []).append((idx, fname, tname, num_lists, style))

    for kind in sorted(by_kind):
        entries = by_kind[kind]
        num_lists = entries[0][3]

        if num_lists == 1:
            # Only one list in this type — no need for childIndex check.
            style = entries[0][4]
            names = ", ".join(fname for _, fname, _, _, _ in entries)
            lines.append(
                f"        case SyntaxKind::{kind}: return ListStyle::{style};"
                f" // {names}"
            )
        elif len(entries) == 1:
            idx, fname, tname, _, style = entries[0]
            lines.append(
                f"        case SyntaxKind::{kind}:"
                f" return listIndex == {idx} ? ListStyle::{style} : ListStyle::Inline;"
                f" // {fname}"
            )
        else:
            # Multiple vertical/dynamic lists in the same type.
            # Group by style.
            by_style = {}
            for idx, fname, tname, _, style in entries:
                by_style.setdefault(style, []).append((idx, fname))

            names = ", ".join(fname for _, fname, _, _, _ in entries)

            # If all entries share the same style, use a simple condition.
            if len(by_style) == 1:
                style = list(by_style.keys())[0]
                indices = " || ".join(
                    f"listIndex == {idx}" for idx, _ in by_style[style]
                )
                lines.append(
                    f"        case SyntaxKind::{kind}:"
                    f" return ({indices}) ? ListStyle::{style} : ListStyle::Inline;"
                    f" // {names}"
                )
            else:
                # Mixed styles — emit if/else chain.
                lines.append(f"        case SyntaxKind::{kind}: // {names}")
                for style, style_entries in by_style.items():
                    indices = " || ".join(
                        f"listIndex == {idx}" for idx, _ in style_entries
                    )
                    lines.append(
                        f"            if ({indices}) return ListStyle::{style};"
                    )
                lines.append("            return ListStyle::Inline;")

    lines.append("        default: return ListStyle::Inline;")
    lines.append("    }")
    lines.append("}")
    return "\n".join(lines)


def generate_attribute_list_function(results):
    """Generate isNonstandardAttributeListSyntax() implementation.

    This only covers types that don't inherit from MemberSyntax,
    StatementSyntax, or ExpressionSyntax. Those are handled by
    isAttributeList() in FormatStyle.cpp using isKind() checks.
    """
    lines = [
        "bool isNonstandardAttributeListSyntax(SyntaxKind parentKind) {",
        "    switch (parentKind) {",
    ]

    for kind in results:
        lines.append(f"        case SyntaxKind::{kind}:")

    lines.append("            return true;")
    lines.append("        default: return false;")
    lines.append("    }")
    lines.append("}")
    return "\n".join(lines)


def generate(types, kindmap):
    """Generate the full output file content."""
    vertical_results = find_list_styles(types, kindmap)
    attr_results = find_attribute_lists(types, kindmap)

    parts = [
        "// Auto-generated by scripts/gen_format_style.py -- do not edit.",
        "",
        generate_vertical_list_function(vertical_results),
        "",
        generate_attribute_list_function(attr_results),
        "",
    ]
    return "\n".join(parts)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check",
        action="store_true",
        help="Check that the generated file matches the generated output",
    )
    parser.add_argument(
        "--write",
        action="store_true",
        help="Write generated output to FormatStyleGen.inc",
    )
    args = parser.parse_args()

    types, kindmap = parse_syntax(SYNTAX_TXT)
    generated = generate(types, kindmap)

    if args.check or args.write:
        try:
            with open(OUTPUT_FILE) as f:
                old_content = f.read()
        except FileNotFoundError:
            old_content = None

        if old_content == generated:
            if args.check:
                print("FormatStyleGen.inc is up to date")
            return
        elif args.check:
            print("FormatStyleGen.inc is out of date. Run:")
            print("  python3 scripts/gen_format_style.py --write")
            sys.exit(1)
        else:
            with open(OUTPUT_FILE, "w") as f:
                f.write(generated)
            print(f"Updated {OUTPUT_FILE}")
    else:
        print(generated)


if __name__ == "__main__":
    main()
