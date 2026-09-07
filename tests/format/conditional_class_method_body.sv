// Conditional statement fragments do not change their enclosing method's indentation.
class foo;
function void first();
endfunction

// Next method.
virtual function void second();
`ifdef FEATURE
if (enabled) begin
`endif
value = other;
`ifdef FEATURE end
`endif
endfunction
endclass
