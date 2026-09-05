class cov_test;
    logic [7:0] addr;
    logic [3:0] cmd;
    logic clk;

    covergroup cg @(posedge clk);
        addr_cp: coverpoint addr {
            bins low    = {[0:63]};
            bins mid    = {[64:191]};
            bins high   = {[192:255]};
            bins zero   = {0};
            bins max    = {255};
        }

        cmd_cp: coverpoint cmd {
            bins read   = {4'b0001};
            bins write  = {4'b0010};
            bins idle   = {4'b0000};
            bins others = default;
        }

        addr_x_cmd: cross addr_cp, cmd_cp {
            ignore_bins ig = binsof(addr_cp.zero) && binsof(cmd_cp.write);
        }
    endgroup

    function new();
        cg = new();
    endfunction
endclass
