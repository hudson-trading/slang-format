// Test: macro invocation as condition in a wrapped ternary expression.
// When the expression wraps, the macro must not be duplicated.

class test_cls;
    task run();
        logic [255:0] masked_data;
        logic [7:0] full_data[8];
        for (int i = 0; i < 4; i++) begin
            if (item.a_mask[i]) begin
                if (item.a_mask[i]) begin
                    masked_data =
                        `gmv(ral.cfg_shadowed.msg_endianness) ? {full_data[i], masked_data} : {masked_data, full_data[i]};
                end
            end
        end
    endtask
endclass
