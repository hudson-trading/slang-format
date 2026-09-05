// Idempotency: when a parameter/data declaration has a macro on the RHS,
// the declarator subtree goes through emitVerbatimNode (because the
// expression placeholder is empty + carries the macro directive trivia).
// Earlier, emitVerbatimNode copied the source span verbatim including
// any user whitespace between tokens — which meant pass 1 saw the
// pre-alignment source spacing and pass 2 saw the formatter's aligned
// spacing, producing different widths in the alignment-speculation
// trial and a different per-row noAlign decision on each pass. Now
// emitVerbatimNode normalizes inter-token whitespace runs to a single
// space (leaving leading indent, comments, and string literals
// untouched) so the trial sees the same widths regardless of source
// spacing.

package wide_ws_pkg;
    parameter int short_name = `MACRO_A;
    parameter SomeLongPackage_pkg::very_long_type_e long_name = `MACRO_B;
    parameter int another_short_name = `MACRO_C;
    parameter int yet_another = `MACRO_D;
endpackage
