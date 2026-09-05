package test_pkg;

    `DECLARE_ANALYSIS_PORT(myPort)
    class TestHelper #(
        parameter int NUM = 4
    ) extends BaseComponent;

        function void build_phase();
        endfunction

    endclass

endpackage
