// Regression: generate-if at module body with labeled blocks containing struct typedefs
// and always_comb blocks. `end else begin :label` must stay on one line.

module top;
    if (support_multi == 0) begin : gen_no_multi
        typedef struct packed {
            lookup_prefix_t prefix;
            lookup_value_t  key;
        } lookup_key_t;

        lookup_key_t lookup_key;

        always_comb begin
            lookup_key = '{
                prefix: ctrl.prefix,
                key:    key_t'(buffer[0].value)
            };

            program_lookup = next_address;
            lookup         = lookup_key;
        end
    end else begin : gen_multi
        typedef struct packed {
            program_index_t program_index;
            lookup_prefix_t prefix;
            lookup_value_t  key;
        } lookup_key_t;

        lookup_key_t lookup_key;

        always_comb begin
            lookup_key = '{
                program_index: start_program_index,
                prefix:        ctrl.prefix,
                key:           key_t'(buffer[0].value)
            };

            program_lookup = {start_program_index, next_address};
            lookup         = lookup_key;
        end
    end
endmodule
