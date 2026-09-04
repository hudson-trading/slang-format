// A standalone macro on the line before a declaration must not be joined to
// the declaration when slang attaches both to one recovered member.
class foo;
    `REGISTER_OBJECT(foo)
    `END_REGISTRATION

    `CONSTRUCT_OBJECT
    bit enabled;
endclass
