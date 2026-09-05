// Single line/block trailing comments are aligned as their own column
// within an alignment group. Each row's comment lands at
//   startCol + max(content_end) + (comma ? 1 : 0) + spacesBeforeTrailingComment
// so even the last member of a comma-separated list (which has no comma)
// keeps its `//` lined up with the others.

module test (
    // Port list: comma-separated. Last port has no comma but the comment
    // still aligns with the others.
    input logic clk,  // clock
    input logic rst,  // synchronous reset
    output logic [7:0] q  // 8-bit output
);
    // Local declarations: `;`-terminated (no separator). Comment column
    // depends on each row's `;` end, padded to the longest.
    logic [7:0] count;  // running counter
    logic enable;  // gate signal
    logic error;  // sticky error

    // Mixed-width assignments inside an always block.
    always_ff @(posedge clk) begin
        count <= count + 1;  // bump
        enable <= !error;  // re-enable while clean
    end

    // Instance with named-port connections (comma-separated again).
    sub
      u_sub (
        .a(signal_a),  // input A
        .b(signal_b),  // input B
        .out(result_wire)  // last conn, no comma
    );

    // Rows without comments stay flush — no phantom trailing whitespace
    // padded to the comment column.
    logic alpha;
    logic beta;  // only this row has a comment
    logic gamma;

    // A wide UNCOMMENTED row must not push a short commented row's comment
    // out to clear it. The comment sits just past the widest *commented*
    // row's own content, not the widest row in the group.
    always_ff @(posedge clk) begin
        ndata_01 <= ip_total_length;
        ndata_23 <= '0;  // identification
    end
endmodule
