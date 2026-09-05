// When a case-item's aligned-column padding pushes its line past the
// column limit, the per-row speculation should pick the standalone
// layout: emit the clause on a new indented line under the label.
// Case-items in the same alignment group whose lines still fit at the
// aligned column stay aligned and on one line.

module test;
    logic [31:0] read_data;
    always_comb begin
        unique case (reg_offset)
            // Group A: short labels with short clauses, all fit at the
            // group's natural width -- alignment stays.
            MacCfg_pkg::REG_CONTROL: read_data = val_control;
            MacCfg_pkg::REG_STATUS:  read_data = val_status;
            MacCfg_pkg::REG_RESET:   read_data = val_reset;
            // One wider label + wide clause -- aligned would overflow,
            // so this case-item drops its clause onto the next line.
            MacCfg_pkg::REG_CONFIG_FILTER_WITH_LONG_NAME: read_data = Bus_pkg::bus_data_t'(val_config);
            default: read_data = '0;
        endcase
    end
endmodule
