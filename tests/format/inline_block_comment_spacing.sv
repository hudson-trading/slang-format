// Block-comment spacing must not change when its surrounding code wraps.
module foo;
reg [7:0] /* status */ value;
reg [0:1] a; /* first */ reg [0:2] b; /* second */ reg [0:3] c;
initial begin
  case (data) inside
    4'b100?:/* branch */ {a,b} = {1'b1,5'h08};
    default:/* other */ {a,b} = '0;
  endcase
  #1 /* settle */ $display("%d", value);
end
endmodule
