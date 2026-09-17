// Mixed declaration types retain alignment when every padded row fits,
// including a row with a wider unpacked dimension.

module test;
    logic sample_valid;
    vector_pkg::branch_ids_t branch;
    vector_pkg::client_id_t client;
    vector_pkg::sample_t samples[vector_pkg::NUM_SAMPLES-1:0];
endmodule
