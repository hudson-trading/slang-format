module generate_test #(
    parameter int N_CHANNELS = 4,
    parameter int WIDTH = 8
) (
    input logic clk,
    input logic rst_n,
    input logic [WIDTH-1:0] data_in[N_CHANNELS],
    output logic [WIDTH-1:0] data_out[N_CHANNELS]
);
    genvar i;
    generate
        for (i = 0; i < N_CHANNELS; i++) begin : gen_channel
            logic [WIDTH-1:0] pipe_reg;

            always_ff @(posedge clk or negedge rst_n) begin
                if (!rst_n)
                    pipe_reg <= '0;
                else
                    pipe_reg <= data_in[i];
            end

            assign data_out[i] = pipe_reg;
        end
    endgenerate

    generate
        if (N_CHANNELS > 2) begin : gen_extra
            logic overflow;
            assign overflow = |data_out;
        end else begin : gen_simple
            logic underflow;
            assign underflow = &data_out;
        end
    endgenerate

    generate
        case (WIDTH)
            8: begin : gen_byte
                logic byte_mode;
                assign byte_mode = 1'b1;
            end
            16: begin : gen_half
                logic half_mode;
                assign half_mode = 1'b1;
            end
            default: begin : gen_other
                logic other_mode;
                assign other_mode = 1'b1;
            end
        endcase
    endgenerate
endmodule
