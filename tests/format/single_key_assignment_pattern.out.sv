// Single keyed assignment patterns stay compact; multiple fields and comments keep their layout.
module single_key_assignment_pattern;
    always_ff @(posedge clk) begin
        state           <= '{default: '0};
        named           <= '{value: 1};
        typed           <= record_t'{default: '0};
        nested          <= '{payload: '{default: '0}};
        nested_multiple <= '{payload: '{
            value:   1,
            default: '0
        }};
        multiple <= '{
            value:   1,
            default: '0
        };
        prefix <= '{  // Clear all entries.
            default: '0
        };
        inline_comment <= '{default: /* Clear */ '0};
        long_value     <= '{value: first_long_signal_name + second_long_signal_name
                                      + third_long_signal_name + fourth_long_signal_name};
        commented <= '{
            // Keep the explanation with the default value.
            default: '0
        };
        trailing <= '{
            default: '0  // Clear all entries.
        };
        closing <= '{
            default: '0
            // End of the pattern.
        };
        preserved <= '{
            // slang-format: off
            default:   '0
            // slang-format: on
        };
    end
endmodule
