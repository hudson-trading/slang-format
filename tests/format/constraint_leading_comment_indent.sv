// Leading comments and their constraint stay at the constraint block's item indentation.
class example;
rand int low, high;
constraint values_c {
low > 0;

// Explain the following bound.
// Its continuation stays aligned.
low < high;

// A single-line section comment has the same behavior.
high > low;

// A long commented constraint still indents genuine expression continuations.
low < first_component + second_component + third_component + fourth_component + fifth_component + sixth_component;
}
endclass
