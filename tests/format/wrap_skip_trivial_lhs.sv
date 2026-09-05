// Regression: a binary operator whose
// LHS is a single literal/identifier (< 4 chars) is a poor break point
// — breaking there saves no real width and leaves a near-empty line
// above the operator. findBreakPoint now skips such candidates so the
// caller falls back to other wrapping strategies (assignment break,
// list-style wrap inside the call).
//
// Before:                              After:
//   localparam x =                      localparam x =
//       1                                   1 << $clog2(some_long_call(...));
//       << $clog2(some_long_call(...));

package test_pkg;
    // Trivial-LHS shift inside a long localparam RHS: the `1` should
    // stay attached to `<<` rather than floating on its own line.
    localparam int num_output_credits = 1 << $clog2(QueueConfig_pkg::getDepth(QueueConfig_pkg::DEFAULT_QUEUE));

    // Trivial-LHS addition: same idea — `5 +` should stay together.
    localparam int total_offset = 5 + computeOffset(QueueConfig_pkg::getDepth(QueueConfig_pkg::DEFAULT_QUEUE));

    // Even with a multi-token shift LHS, the member DP prefers the shallower
    // assignment boundary when that is enough to fit the line.
    localparam int shifted = (alpha + beta) << shift_amount_that_is_intentionally_kind_of_long_too_so_it_must_wrap;
endpackage
