// Inline conditional directives retain class-member indentation across passes.
class foo;
int a;
`ifndef FEATURE int b; `endif
int c;
endclass
