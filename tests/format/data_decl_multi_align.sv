// Multi-declarator data decls participate in alignment. Type+packed-dim
// column pads to the longest so the first declarator name in each row starts
// at the same column. 2nd, 3rd, ... declarator names also align across rows
// when the rows have matching declarator count — padding lands AFTER the
// comma (before the next name), keeping commas flush with their preceding
// identifier. Mixed declarator counts collapse the trailing column (still
// aligning the type column).

module top;
    // Same shape (2 declarators each) — full sub-tree alignment.
    logic [15:0] next_byte_count, byte_count;
    logic [15:0] next_max_bytes, max_bytes;
    logic [2:0]  next_data_offset, data_offset;
    logic [63:0] next_rx_packet_data_stored, rx_packet_data_stored;

    // Mixed single/multi — type column still aligns, trailing collapses.
    logic [7:0] a, b;
    logic [7:0] c;
    logic [7:0] d, e, f;
endmodule
