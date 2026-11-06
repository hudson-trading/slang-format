// Runs of empty lines are reduced to maxEmptyLines (1 here) everywhere.

module top #(
    parameter int A = 1,

    parameter int B = 2
) (
    input  logic clk,

    // comment after a run
    input  logic rst,

    output logic q
);

    logic a;

    logic b;

    // standalone comment

    `ifdef FOO

        logic c;

    `endif

    always_ff @(posedge clk) begin
        a <= 1'b0;

        b <= 1'b1;
    end

    /* block comment

       keeps its own empty line */

    child
      u_child (
        .clk (clk),

        .rst (rst),
        .q   (q)
    );

endmodule

module second;
endmodule
