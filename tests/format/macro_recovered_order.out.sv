// Recovered declaration prefixes must stay before their macro qualifiers.
class foo;
    extern local `QUALIFIER task bar(int value);
endclass
// Sized literals supplied by macros must stay after the conditional operator.
wire [`WIDTH-1:0] value = flag ? `WIDTH 'h1
: `WIDTH'h3;
