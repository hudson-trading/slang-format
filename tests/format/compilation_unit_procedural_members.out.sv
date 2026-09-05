// Include fragments can contain procedural members that are parsed standalone
// at compilation-unit scope. Their syntax is complete and should be formatted.
always_comb begin
    lhs        = rhs;
    longer_lhs = value;
end

always_ff @(posedge clk) begin
    registered <= lhs;
end
