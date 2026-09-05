// Regression: when a case-item clause
// ends up on its own line — either because the labels wrapped, the
// clause doesn't fit after `:`, or the source already placed it on a
// new line — the clause must be indented one level under the labels.
// Previously the clause landed at the same column as the labels,
// making it look like a sibling case-item header rather than a body.

package test_pkg;
    // Source has each label on its own line. After formatting the labels
    // get merged onto one line (Dynamic), but the clause must still be
    // indented under them.
    function bit isSpecialEvent(int msgid);
        case (msgid)
            MODE_ALPHA,
            MODE_BETA,
            MODE_GAMMA_EXTENDED,
            MODE_DELTA:
                return '1;
            default:
                return '0;
        endcase
    endfunction

    // Labels fit on one line and short clause stays on same line: no break.
    function bit short_clause(int msgid);
        case (msgid)
            A, B, C: return '1;
            default: return '0;
        endcase
    endfunction

    // Labels fit on one line but clause is too long: clause breaks and
    // indents under the labels.
    function bit long_clause(int msgid);
        case (msgid)
            MODE_ALPHA,
            MODE_BETA,
            MODE_GAMMA_EXTENDED,
            MODE_DELTA:
                very_long_function_call(thing, other_thing, more_things);
            default:
                return '0;
        endcase
    endfunction
endpackage
