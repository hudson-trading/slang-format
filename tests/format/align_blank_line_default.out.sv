// List alignments (case items, struct-pattern assigns, struct fields) use
// listGapLines (default 2): a single blank keeps them aligned.
// Statements and declarations use statementGapLines (default 1),
// so a single blank splits those groups.

package align_blank_pkg;
    // Struct fields: single blank line keeps them aligned.
    typedef struct packed {
        logic       valid;
        logic [7:0] data;

        logic       error;
        logic [3:0] code;
    } status_t;
endpackage

module test;
    // Local data decls (body alignment): single blank line splits the group,
    // so the `[3:0]` row aligns independently from the scalar rows above.
    logic ready;
    logic valid;

    logic [31:0] payload;
    logic [3:0]  count;

    always_comb begin
        // Case items: single blank line keeps the alignment group intact.
        unique case (opcode)
            OP_ALPHA:   result = src_a;
            OP_BETA:    result = src_b;

            OP_GAMMA:   result = src_c;
            OP_DEFAULT: result = '0;
        endcase

        // Struct-pattern assigns: single blank line keeps the field
        // alignment intact.
        status_word = '{
            valid: 1'b1,
            data:  alpha_in,

            error: 1'b0,
            code:  beta_in
        };

        // Assignment statements (body): single blank splits the group.
        // The second group aligns independently of the first.
        x         = 1;
        long_name = 2;

        z  = 3;
        zz = 4;
    end
endmodule
