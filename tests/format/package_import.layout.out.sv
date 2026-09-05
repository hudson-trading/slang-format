package types_pkg;
    typedef enum logic [1:0] {
        CMD_READ = 2'b00,
        CMD_WRITE = 2'b01,
        CMD_IDLE = 2'b10
    } cmd_t;

    typedef struct packed {
        cmd_t cmd;
        logic [7:0] addr;
        logic [7:0] data;
    } req_t;

    function automatic logic [7:0] encode(req_t r);
        return r.addr ^ r.data;
    endfunction
endpackage

module pkg_user
    import types_pkg::*;
(
    input logic clk,
    input req_t req_in,
    output logic [7:0] encoded
);
    always_ff @(posedge clk) begin
        encoded <= encode(req_in);
    end
endmodule
