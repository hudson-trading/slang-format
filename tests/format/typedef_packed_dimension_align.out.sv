// Typedefs with packed dimensions align both the dimension and alias columns
// when their base types have different widths.
package typedef_packed_dimension_align;
    typedef logic  [$clog2(block_width)-1:0] block_index_t;
    typedef logic  [payload_width-1:0]       payload_t;
    typedef logic  [check_width-1:0]         check_t;
    typedef logic  [raw_width-1:0]           raw_t;
    typedef logic  [word_width-1:0]          word_t;
    typedef word_t [word_count-1:0]          words_t;
    typedef logic  [address_width-1:0]       address_t;
    typedef logic  [select_width-1:0]        select_t;
endpackage
