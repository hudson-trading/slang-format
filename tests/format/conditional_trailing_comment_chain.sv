// A same-line comment after `endif belongs to that directive even when slang
// exposes it as leading sub-trivia of the following conditional directive.
module conditional_trailing_comment_chain;
`ifdef FIRST_FEATURE
logic first_value;
`endif // FIRST_FEATURE
`ifdef SECOND_FEATURE
logic second_value;
`endif

`ifdef THIRD_FEATURE
logic third_value;
`endif // THIRD_FEATURE

`ifndef FOURTH_FEATURE
logic fourth_value;
`endif

`ifdef FIFTH_FEATURE
logic fifth_value;
`endif
// This standalone comment belongs to the following conditional region.
`ifdef SIXTH_FEATURE
logic sixth_value;
`endif
endmodule
