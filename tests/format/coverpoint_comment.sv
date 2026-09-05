// Test: comment before coverpoint should not be duplicated

covergroup test_cg;
    // clear coverpoint
    clear_cp: coverpoint clear {
        bins clear_all = {[4:$]};
    }
    // state coverpoint
    state_cp: coverpoint state
    ;
endgroup
