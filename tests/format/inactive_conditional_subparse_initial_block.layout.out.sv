// Parse inactive conditional members so procedural block contents receive normal indentation.
module inactive_conditional_subparse_initial_block;
    `ifdef OPTIONAL_FEATURE
        logic report_data;

        `ifdef OPTIONAL_REPORT
            initial begin
                #0;
                report_value(report_data);
            end
        `endif
    `endif
endmodule
