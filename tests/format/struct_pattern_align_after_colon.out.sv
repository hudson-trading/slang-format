// Regression: in struct-assignment patterns,
// when a value expression wraps onto continuation lines, those lines
// should start one indent past the aligned value expression.

package test_pkg;
    typedef struct packed {
        int   record_kind;
        int   sequence_number;
        int   tag;
        logic is_last;
        int   item_id;
    } record_t;

    always_comb begin
        record_data = '{
            record_kind:     RECORD_DATA,
            sequence_number: record_metadata.token.sequence_number,
            tag:             record_metadata.token.tag,
            is_last:         record_output.data.is_last
                                && record_pkg::isFinalPhase(record_output.data.phase),
            item_id:         record_metadata.common.item_id
        };
    end
endpackage
