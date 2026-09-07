// Compact dimensions and unary chains must preserve distinct operator tokens.
module foo;
logic [15:0] values;
int i, j;
assign x = - -i;
assign y = + +j;
assign z = ~ &values;
initial begin
i = values[i + +j];
i = values[i - -j];
i = values[i & &j];
i = values[i | |j];
i = values[i + j];
i = values[i && j];
end
endmodule
