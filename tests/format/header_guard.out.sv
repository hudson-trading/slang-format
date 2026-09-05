`ifndef MY_HEADER_SVH
    `define MY_HEADER_SVH

    typedef enum logic [1:0] {
        STATE_IDLE = 2'b00,
        STATE_RUN  = 2'b01,
        STATE_DONE = 2'b10
    } state_t;

    `define REG_WIDTH 32
    `define MAKE_REG(name) logic [`REG_WIDTH-1:0] name

`endif
