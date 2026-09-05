// Regression: identifiers starting with a keyword (integer_t, logic_t, byte_t)
// after an unknown macro produced phantom keyword tokens in SkippedTokens.
module macro_keyword_cast;
    `M(x) = integer_t'(a);
    `M(x) = logic_t'(a);
    `M(x) = byte_t'(a);
endmodule
