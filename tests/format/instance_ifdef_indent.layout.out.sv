module instance_ifdef_indent;
    `ifdef USE_SLOW_CLOCK
        localparam bit slow_clock_enabled=1;
        ControlBus #(.address_width(28))
        bus_0 (.clk(clk_slow), .rst(rst_slow)),
        bus_1 (.clk(clk_slow), .rst(rst_slow)),
        bus_2 (.clk(clk_slow), .rst(rst_slow)),
        bus_3 (.clk(clk_slow), .rst(rst_slow));

        ControlCdc
        cdc_0(.sink(endpoints[0]), .source(bus_0)),
        cdc_1(.sink(endpoints[1]), .source(bus_1)),
        cdc_2(.sink(endpoints[2]), .source(bus_2)),
        cdc_3(.sink(endpoints[3]), .source(bus_3));
    `else
        localparam bit slow_clock_enabled = 0;
        ControlBus #(.address_width(28))
          bus_0 (.clk(clk), .rst(rst)),
          bus_1 (.clk(clk), .rst(rst)),
          bus_2 (.clk(clk), .rst(rst)),
          bus_3 (.clk(clk), .rst(rst));
    `endif

    logic separator;

    `ifdef USE_NETLIST
        `include "generated_netlist.svh"
    `else
        (* `PRESERVE_BLOCK *)
        SignalProcessor #(
            .update_latency(4),
            .max_cycles(4),

            .pre_cycles(4),

            `CONFIG_PARAM_MAP(output_cfg, output_cfg)
        ) u_processor (
            .control(control_ports[CONTROL]),
            .sorter_control(control_ports[SORTER_CONTROL]),
            .*
        );
    `endif
endmodule
