// A member selection remains attached to its macro-expanded root even when a
// previous formatter pass split the source between the macro and dot.
`define ROOT top
`define BRANCH unit

module example;
    assign combined = {`ROOT
        .subsystem.channel_one.enable,
        `ROOT.subsystem.channel_zero.enable
    };

    task probe();
        $sample(
            0,
            `ROOT.`BRANCH.state
        );
    endtask
endmodule
