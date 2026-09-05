// Port grouping with a macro-typed first port. Slang recovers `output `T a`
// by parsing the macro as a placeholder type and pushing `a` into the
// following comma's SkippedTokens; `b` becomes a separate grouped port.
// The verbatim path emits only the first port's source range, so taking it
// here would silently drop the grouped siblings (b, c). Stay on the normal
// formatting path when an entry has additional grouped node items.
module m (
    output `T a, b, c,
    output `WIDE_T x0, x1, x2
);
endmodule
