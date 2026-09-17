// Calls in type specifications and array dimensions stay inline even beyond the column limit.
module demo #(
    parameter int BASE_WIDTH = 8
) (
    input logic [BASE_WIDTH+$bits(example_types_pkg::extended_metadata_record_t)-1:0] input_value
);
    typedef logic [configured_index_width+$bits(example_types_pkg::extended_metadata_record_t)-1:0] entry_t;
    logic [configured_index_width+$bits(example_types_pkg::extended_metadata_record_t)-1:0] stored_value;


    entry_t entries[calculate_entry_count(example_config_pkg::maximum_supported_entry_count)];

    // Ordinary calls still wrap, including calls that take a type as their argument.
    initial begin
        source_group.endpoints[index].extended_metadata_analysis_port.connect(
            handlers[index].extended_metadata_analysis_imp
        );
        $display($bits(
                example_types_pkg::extended_metadata_record_with_a_deliberately_long_name_t
            ));
    end
endmodule
