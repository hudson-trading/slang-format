module m;
    // Short: stays inline
    function void f(Pkg::Type #(W) a, Other::Type b);
    endfunction

    // Medium: wraps vertically
    function void setup(
        Util_pkg::LookupTable lookupTable,
        Bus_pkg::Requestor #(addr_width) requestor,
        query_t query
    );
    endfunction

    // Long: wraps vertically with array dimensions
    function void configure(
        AaaLongPkg_pkg::TableConfig tableConfig,
        Bus_pkg::Requestor #(addr_width) requestor,
        Proto_pkg::proto_t protos[Cfg_pkg::num_channels],
        query_t query
    );
    endfunction
endmodule
