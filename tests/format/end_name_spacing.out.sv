// End-name colons are separated from their closing keywords without changing ranges.
module foo;
    logic [3:0] value;
    function void bar();
        begin : baz
            value = 0;
        end : baz
    endfunction : bar
endmodule : foo
