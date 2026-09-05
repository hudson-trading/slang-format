// Test: macro invocation in constraint expression
// The macro should not be dropped from the output

class test_cls;
    rand int unsigned baud_rate;

    constraint baud_rate_c {
        // constrain nco not over limit
        `CALC_NCO(baud_rate, cfg.ral.ctrl.nco.get_n_bits(),
            cfg.clk_freq_mhz) < 2** cfg.ral.ctrl.nco.get_n_bits();
    }
endclass
