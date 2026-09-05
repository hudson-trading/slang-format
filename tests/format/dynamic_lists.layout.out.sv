// Module port list is always vertical
module short_ports (
    input logic a,
    output logic b
);
    assign b = a;
endmodule

// Module port list with many ports is also vertical
module long_ports (
    input logic [31:0] very_long_signal_name_alpha,
    input logic [31:0] very_long_signal_name_beta,
    output logic [31:0] very_long_result_out
);
    assign very_long_result_out = very_long_signal_name_alpha + very_long_signal_name_beta;
endmodule

// Port list with line comment goes vertical
module comment_ports (
    input logic a,  // first input
    output logic b
);
    assign b = a;
endmodule

// Port list with block comment goes vertical
module block_comment_ports (
    input logic a,  /* first input */
    output logic b
);
    assign b = a;
endmodule

// Parameter override is always vertical
module param_inst;
    child #(.W(8))
      u_inst (.a(x), .b(y));
endmodule

// Multiple parameter overrides also vertical
module param_inst_long;
    child #(
        .VERY_LONG_PARAM_NAME_A(some_value_a),
        .VERY_LONG_PARAM_NAME_B(some_value_b),
        .VERY_LONG_PARAM_NAME_C(some_value_c)
    ) u_inst (.a(x));
endmodule

// Single instance connection stays inline
module conn_single;
    child
      u_inst (.a(x));
endmodule

// Multiple instance connections go vertical
module conn_inst;
    child
      u_inst (.a(x), .b(y));
endmodule

// Instance connection with comment goes vertical
module conn_comment;
    child
      u_inst (
        .a(x),  // first port
        .b(y)
    );
endmodule

// Function with short port list stays inline
module func_mod;
    function automatic logic toggle(input logic x);
        return ~x;
    endfunction
endmodule

// Function with long port list goes vertical
module func_long_mod;
    function automatic logic [31:0] compute_something(
        input logic [31:0] operand_alpha,
        input logic [31:0] operand_beta
    );
        return operand_alpha + operand_beta;
    endfunction
endmodule

// Short concatenation stays inline
module concat_short;
    logic [3:0] out;
    assign out = {a, b};
endmodule

// Long concatenation goes vertical when exceeding column limit
module concat_long;
    logic [127:0] out;
    assign out = {
        signal_name_alpha,
        signal_name_beta,
        signal_name_gamma,
        signal_name_delta,
        signal_name_epsilon
    };
endmodule

// Concatenation with comment goes vertical
module concat_comment;
    logic [3:0] out;
    assign out = {
        a,  // first signal
        b
    };
endmodule

// Replication with inner concatenation
module concat_replication;
    logic [127:0] out;
    assign out = {4 {
        signal_name_alpha,
        signal_name_beta,
        signal_name_gamma,
        signal_name_delta,
        signal_name_epsilon
    }};
endmodule

// Named struct assignment is always vertical
module struct_short;
    typedef struct packed {
        logic [7:0] a;
        logic [7:0] b;
    } pair_t;
    pair_t p;
    assign p = '{
        a: 8'h01,
        b: 8'hFF
    };
endmodule

// Named struct assignment with many fields also vertical
module struct_long;
    typedef struct packed {
        logic [31:0] alpha;
        logic [31:0] beta;
        logic [31:0] gamma;
    } triple_t;
    triple_t t;
    assign t = '{
        alpha: 32'hDEAD_BEEF,
        beta: 32'hCAFE_BABE,
        gamma: 32'h0123_4567
    };
endmodule

// Struct assignment with comment goes vertical
module struct_comment;
    typedef struct packed {
        logic [7:0] a;
        logic [7:0] b;
    } pair_t;
    pair_t p;
    assign p = '{
        a: 8'h01,  // first field
        b: 8'hFF
    };
endmodule
