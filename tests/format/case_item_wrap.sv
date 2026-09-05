// Test: long case item labels should wrap

module test;
    always_comb begin
        unique case (offset)
            // Short labels stay inline
            A, B: y = 1;
            C: y = 0;
            // Long labels wrap
            VeryLongPackage_pkg::CONTROL_LANEA, VeryLongPackage_pkg::CONTROL_LANEB, VeryLongPackage_pkg::CONTROL_LANEC, VeryLongPackage_pkg::CONTROL_LANED: data <= bus_data_t'(control[lanesel]);
            // Long labels with block body
            VeryLongPackage_pkg::STATUS_LANEA, VeryLongPackage_pkg::STATUS_LANEB, VeryLongPackage_pkg::STATUS_LANEC, VeryLongPackage_pkg::STATUS_LANED: begin
                data <= status;
                err <= 0;
            end
            default: err <= 1'b1;
        endcase
    end
endmodule
