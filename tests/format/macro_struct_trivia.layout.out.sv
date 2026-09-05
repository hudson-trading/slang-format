`define LOG_FIELDS \
    logic [7:0] index; // 8 \
    logic [1:0] reason;

module m;
    typedef struct packed {
        logic [filler_width-1:0] filler;
        `LOG_FIELDS // 8 \
    } log_entry_t;
endmodule
