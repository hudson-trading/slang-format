// A packed array of a named type remains valid after a class registration macro.
class foo extends base;
    `REGISTER(foo)

    byte_t [6:0] data;
    byte_t next;
    `FIELD_TYPE field;
    `REGISTER_MORE(foo);
    byte_t [2:0] more;
    function new(string name = "");
        super.new(name);
    endfunction
endclass
