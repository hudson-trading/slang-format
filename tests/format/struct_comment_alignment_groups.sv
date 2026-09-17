// Struct comments align across varying member lengths, regrouping when padding would overflow.
module demo;
typedef struct packed {
sample_pkg::byte_t tag; // Header.
sample_pkg::byte_t extended_mode; // Mode.
index_t slot; // Slot.
logic valid; // Validity.
// Payload fields.
sample_pkg::word_t remaining_word_count; // Count.
sample_pkg::word_t count; // Size.
} record_t;

typedef struct packed {
logic x; // This field has a long description that leaves little room for additional padding.
logic a; // Flag.
logic alternate_flag; // Alternate flag.
logic b; // Another flag.
} flags_t;

typedef union packed {
sample_pkg::word_t word; // Raw word.
logic [31:0] alternate_encoding; // Encoded word.
} value_t;
endmodule
