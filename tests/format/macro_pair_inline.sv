// Two macro invocations on a single source line. Slang's parser
// (recoveryIsStandalone) now treats each unknown macro followed by another
// recovery token as its own EmptyMember instead of fabricating a single
// DataDeclaration whose type/name slots hold both macros as trivia. That
// lets the formatter emit each macro as a normal member and round-trip
// cleanly through the CST.
module m;
    `PAIR_A(a)   `PAIR_B(b)
    `PAIR_A(c)   `PAIR_B(d)

    Foo c1();
endmodule
