// A macro argument split onto its own line must keep the call's continuation anchor.
class example;
    function void build();
        factory_handle = new("a deliberately long factory handle name that forces arguments to wrap", `TYPE_NAME);
    endfunction
endclass
