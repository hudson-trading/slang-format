// Regression: when an instantiation has a single parameter override AND the port
//   connection list is going to wrap vertically, keep the params vertical
//   too so the instance name stays on the `) name (` line. Inlining the
//   single-param header forces a dangling `\n  name (` continuation that
//   looks broken next to neighboring multi-param instances that DID stay
//   vertical.

module parent (
    input logic clk,
    input logic rst,
    output logic out_a,
    output logic out_b
);
    // Single-param + connections that fit inline: keep params inline.
    Mod #(.data_t(logic))
      u_short (.clk(clk), .rst(rst));

    // Single-param + connections that need vertical layout: keep params
    // vertical so we get `) u_long (\n     .conn,` rather than
    // `Mod #(.data_t(logic))\n  u_long (\n      .conn,`.
    Mod #(
        .data_t(logic [31:0])
    ) u_long (
        .write_data(some_long_signal_name_alpha),
        .read_data(some_long_signal_name_beta),
        .write_enable(some_long_signal_name_gamma),
        .read_enable(some_long_signal_name_delta)
    );

    // Multi-param + vertical connections for contrast.
    Mod #(
        .data_t(logic [31:0]),
        .addr_t(logic [7:0])
    ) u_multi (
        .write_data(some_long_signal_name_alpha),
        .read_data(some_long_signal_name_beta),
        .write_enable(some_long_signal_name_gamma),
        .read_enable(some_long_signal_name_delta)
    );
endmodule
