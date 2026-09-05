// The layout stage solves each member before alignment adds only column padding.
module formatter_pipeline_stages;
logic flag;
logic [31:0] result;

assign result = first_long_operand + second_long_operand * third_long_operand + fourth_long_operand * fifth_long_operand;
assign another_result = first_long_operand + second_long_operand * third_long_operand + fourth_long_operand * fifth_long_operand;
endmodule
