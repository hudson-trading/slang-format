# Formatting markers

Use `// slang-format: skip` to preserve the formatting of the next declaration
or statement. The rest of the source is formatted normally.

## Skip one declaration or statement

Place the marker in a comment immediately before the construct to preserve:

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

## At the top of a file

A top-of-file skip marker applies to the first declaration, **not the entire
file**. If that declaration is a module, its whole body is preserved:

```systemverilog
// slang-format: skip
module first;logic a;endmodule
module second;logic b;endmodule
```

The first module stays as written. The second module is formatted normally.
For a file containing only one module or package, skipping that declaration can
therefore preserve nearly all of the file.

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

`skip` preserves formatting; the source is still parsed and validated. It does
not hide syntax errors or merge conflict markers. See [validation](format-validation.md)
for how those diagnostics affect output.

`--force` overrides validation failures and still respects `skip` and generated
file exclusions. Region markers such as `slang-format: off` and `slang-format: on`
are not currently supported.
