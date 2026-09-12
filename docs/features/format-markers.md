# Disabling formatting

## Skip one declaration, statement, or group

Place the marker in a comment immediately before the construct or group to preserve:

```systemverilog
module example;
    // slang-format: skip
    localparam int MASK = (1<<4) | (1<<1);

    localparam int other=2;
endmodule
```


The `MASK` declaration keeps its existing spacing. The `other` declaration is
formatted normally. A skipped declaration also separates the alignment groups
on either side of it.

The marker applies to a complete declaration or statement, including its nested
contents. For example, place it before an `always` block to preserve that whole
block. It is not a general way to skip an arbitrary expression or port-list
entry. Use the exact, case-sensitive spelling `slang-format: skip`; a block
comment containing the same marker is also accepted.

This should generally be preferred over on/off directives, since those can leak.

## Disable a region

Put the markers on their own comment lines, between items in the same list:

```systemverilog
module example;
    // slang-format: off
    localparam int LEFT=1;localparam int RIGHT= 2;
    // slang-format: on
    localparam int normal=3;
endmodule
```

The two declarations in the disabled region retain their internal spacing,
including the space between them. Formatting resumes at `normal`. The formatter
can adjust the indentation at the region boundary and format the marker comments.

Each list has its own off/on state: the file's declarations, module or class
members, a block's statements, or a port or argument list. Markers must sit between
complete items in that list, not inside an expression. Inline lists such as the
names within one variable declaration do not support region markers.

An `on` inside a nested scope cannot close an outer `off`, and an `on` after the
scope ends cannot close an `off` inside it:

```systemverilog
module example;
    initial begin
        // slang-format: off
        a=1;b=2;
    end
    // slang-format: on
    logic c;
endmodule
```

Here the assignments are preserved, but the `on` is in the module's member list,
not the block's statement list. Formatting resumes when the block ends, and the
formatter warns:

```text
slang-format: on has no preceding off in the same list scope. Did you put the off directive in the wrong scope?
```

An unmatched `off` silently preserves the rest of its list. An unmatched `on`
produces the warning above and formatting continues; the warning does not reject
the file or change the exit status. Repeated `off` markers in the same list do
not nest: the next `on` resumes formatting. Both markers also work in standalone
block comments.

## Disable a whole file

Put `off` before the first declaration to preserve every declaration in the file:

```systemverilog
// slang-format: off
module first;logic a;endmodule
module second;logic b;endmodule
```

No final `on` is needed, and this produces no warning. To resume formatting later
in the file, put `on` between top-level declarations; placing it inside a module
would be a different list scope.

## Generated files

For generated source, put `@generated` in a leading comment:

```systemverilog
// @generated
module first;logic a;endmodule
module second;logic b;endmodule
```

The formatter returns the whole file unchanged. A leading block comment works
too. The marker must be in the comments before the first source token;
`@generated` in a later comment or a string is not a file exclusion.

## Interaction with validation and force

`skip` and off/on regions preserve formatting; the source is still parsed and
validated. They do not hide syntax errors or merge conflict markers. See
[validation](format-validation.md) for how those diagnostics affect output.

`--force` overrides validation failures and still respects `skip`, off/on regions,
and generated file exclusions. An unmatched-on warning does not require force.
