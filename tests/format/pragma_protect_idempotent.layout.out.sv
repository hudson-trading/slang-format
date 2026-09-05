// Idempotency: slang's lexer absorbs the trailing newlines after a
// `pragma protect` encoded block into the Unknown skipped-token text.
// If the formatter ALSO emits its own newline+indent before the next
// directive, an extra blank line accumulates on every pass. Sync the
// lineState to whatever the raw text already left behind.
`pragma protect begin_protected
`pragma protect key_keyowner = "X", key_keyname= "y", key_method = "rsa", key_block
ABCdef123==

`pragma protect control xilinx_visible = "false"
`pragma protect end_protected
