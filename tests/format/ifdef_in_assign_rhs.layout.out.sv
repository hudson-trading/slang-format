// Test: ifdef in assign RHS should be idempotent
// Reproduces non-idempotent formatting when assign RHS is split across ifdef

interface test_if;
    // Pattern 1: ifdef wrapping full assign
    wire [7:0] csrng_state;
    `ifdef GATE_LEVEL
        assign csrng_state = 0;
    `else
        assign csrng_state = `CSRNG_HIER.state_q;
    `endif
    `DV_CREATE_SIGNAL_PROBE(signal_probe_csrng,
      csrng_state, 8)
    // Pattern 2: assign with ifdef in RHS
    wire [5:0] aes_state;
    assign aes_state =
    `ifdef GATE_LEVEL
        0;
    `else
        `AES_HIER.aes_ctrl_cs;
`endif
`DV_CREATE_SIGNAL_PROBE(signal_probe_aes,
      aes_state, 6)

// Pattern 3: ifdef with multiline expression
wire [2:0] hmac_state;
`ifdef GATE_LEVEL
    assign hmac_state = {`HMAC_HIER.st_q_reg_2_.Q
                        ,`HMAC_HIER.st_q_reg_1_.Q
                        ,`HMAC_HIER.st_q_reg_0_.Q
                        };
`else
    assign hmac_state = `HMAC_HIER.st_q;
`endif
`DV_CREATE_SIGNAL_PROBE(signal_probe_hmac,
      hmac_state, 3)
endinterface
