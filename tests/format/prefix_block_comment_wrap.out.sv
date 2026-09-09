// A block comment before a nested type expression must stay attached when it wraps.
virtual class foo extends bar;
    function void build_phase(phase_t phase);
        for (int i = 0; i < items.size() && !found; i++) begin
            begin
                begin
                    if ( /* success */ found) begin
                        begin
                            typedef bit [$bits( /* type */ some_long_enumerated_type_for_active_passive_mode)-1:0] value_t;
                        end
                    end
                end
            end
        end
    endfunction
endclass
