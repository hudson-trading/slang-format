// A conditional terminator recovered with a following macro must close the
// surrounding indentation before the next class member is formatted.
class example;
`ifdef FEATURE
bit [3:0] values[];
`else
rand bit [3:0] values[];

constraint values_c {
values.size() == 4;
}
`endif

`REGISTER_TYPE(example)

function new(string name = "");
super.new(name);
endfunction
endclass
