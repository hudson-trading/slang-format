// Macro assertion actions retain their else clauses; a real empty action leaves else to if.
module assertion_macro_else;
    initial begin
        assert (ready)
            `ACTION(data)
        else $error("failed");
        assert (ready)
            `ACTION(data);
        else $error("failed");
        assert (ready)
            `TARGET(data) = value;
        else $error("failed");
        if (enabled)
            assert (ready);
        else
            $error("disabled");
    end
endmodule
