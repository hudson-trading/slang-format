// Single-argument calls can wrap at parentheses when neither hierarchy has a break point.
class demo;
    virtual function void configure();
        for (int i = 0; i < count; i++) begin
            source_group.endpoints[i].extended_metadata_analysis_port.connect(
                handlers[i].extended_metadata_analysis_imp
            );
            source_group.endpoints[i].fallback_metadata_analysis_port.connect(
                handlers[i].fallback_metadata_analysis_imp
            );
            source_group.endpoints[i].short_port.connect(handlers[i].short_imp);
        end
    endfunction
endclass
