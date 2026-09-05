// Macro-containing string concatenations near the column limit must make the
// same alignment and wrapping decisions on every formatter pass.
module localparam_string_align_idempotent;
    localparam string valid_fmt = "valid=%BOOL";
    localparam string offset_sum_fmt = {
        "hdr+dataOffset=%",
        `FORMAT_WIDTH("%0du", $bits(offset_t))
    };
    localparam string offset_fmt = {
        "offset=%",
        `FORMAT_WIDTH("%0du", $bits(offset_t))
    };
    localparam string header_offset_fmt = {
        "header_offset=%",
        `FORMAT_WIDTH("%0du", $bits(header_offset_t))
    };
    localparam string block_id_fmt = {
        "block_id=%",
        `FORMAT_WIDTH("%0du", $bits(block_id_t))
    };
    localparam string length_fmt = {
        "extra_length=%",
        `FORMAT_WIDTH("%0du", $bits(Networking_pkg::ip_total_len_t))
    };
    localparam string time_base_fmt = {
        "timeBase=%",
        `FORMAT_WIDTH("%0du", $bits(ProcCompact_pkg::timeBase_t))
    };
    localparam string meta_fmt = "meta=%x";
    localparam string descriptor_fmt = {
        valid_fmt,
        " ",
        offset_sum_fmt,
        " ",
        offset_fmt,
        " ",
        header_offset_fmt,
        " ",
        block_id_fmt,
        " ",
        meta_fmt
    };
endmodule
